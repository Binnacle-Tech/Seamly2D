/******************************************************************************
 *   @file   dxfreader.cpp
 **  @author Seamly2D project
 **  @date   16 10, 2026
 **
 **  @brief  Reads the drawing in a DXF file: blocks, inserts, lines, polylines, curves and text.
 **
 **  @copyright
 **  Copyright (C) 2026 Seamly2D project
 **  <https://github.com/fashionfreedom/seamly2d> All Rights Reserved.
 **
 **  Seamly2D is free software: you can redistribute it and/or modify
 **  it under the terms of the GNU General Public License as published by
 **  the Free Software Foundation, either version 3 of the License, or
 **  (at your option) any later version.
 **
 **  Seamly2D is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU General Public License for more details.
 **
 **  You should have received a copy of the GNU General Public License
 **  along with Seamly2D.  If not, see <http://www.gnu.org/licenses/>.
 **
 *****************************************************************************/

#include "dxfreader.h"

#include <QFile>
#include <QIODevice>
#include <QRegularExpression>
#include <QtMath>

#include <limits>

namespace Seamly3D
{

namespace
{

//---------------------------------------------------------------------------------------------------------------------
struct Group
{
    int     code;
    QString value;
};

//---------------------------------------------------------------------------------------------------------------------
double toDouble(const QString &value)
{
    bool ok = false;
    const double result = value.trimmed().toDouble(&ok);
    return ok ? result : 0.0;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief decodeText turns DXF text escapes into characters: \U+00E9, %%d (degree), %%p (plus/minus), %%c (diameter). */
QString decodeText(QString text)
{
    static const QRegularExpression unicode(QStringLiteral("\\\\U\\+([0-9A-Fa-f]{4})"));
    QRegularExpressionMatch match = unicode.match(text);
    while (match.hasMatch())
    {
        text.replace(match.capturedStart(), match.capturedLength(),
                     QChar(static_cast<char16_t>(match.captured(1).toUShort(nullptr, 16))));
        match = unicode.match(text);
    }
    text.replace(QStringLiteral("%%d"), QStringLiteral("°"), Qt::CaseInsensitive);
    text.replace(QStringLiteral("%%p"), QStringLiteral("±"), Qt::CaseInsensitive);
    text.replace(QStringLiteral("%%c"), QStringLiteral("⌀"), Qt::CaseInsensitive);
    return text;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief plainMText removes the formatting codes of MTEXT: fonts, colors, heights; \P is a new line. */
QString plainMText(QString text)
{
    text.replace(QStringLiteral("\\P"), QStringLiteral("\n"));
    static const QRegularExpression codes(QStringLiteral("\\\\[A-Za-z][^;\\\\{}]*;"));
    text.remove(codes);
    text.remove(QLatin1Char('{'));
    text.remove(QLatin1Char('}'));
    return text;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief Number of segments for an arc of @p angle radians and radius @p radius to stay within @p tolerance. */
int arcSegments(double radius, double angle, double tolerance)
{
    if (radius <= tolerance || tolerance <= 0)
    {
        return qMax(1, qCeil(qAbs(angle) / (M_PI / 8)));
    }
    const double step = 2 * std::acos(qBound(-1.0, 1 - tolerance / radius, 1.0));
    return qBound(1, qCeil(qAbs(angle) / qMax(step, 1e-3)), 720);
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief bulgeArc appends the points of the arc from @p a to @p b with @p bulge (tan of a quarter of its angle),
 *  without @p a, with @p b. */
void bulgeArc(QPolygonF &out, const QPointF &a, const QPointF &b, double bulge, double tolerance)
{
    if (qAbs(bulge) < 1e-9)
    {
        out.append(b);
        return;
    }
    const double angle = 4 * std::atan(bulge);
    const double chord = QLineF(a, b).length();
    if (chord < 1e-12)
    {
        out.append(b);
        return;
    }
    const double radius = chord / (2 * std::sin(qAbs(angle) / 2));
    // The center is on the perpendicular bisector of the chord: on the left of a counterclockwise (positive) arc under
    // a half turn, on the right of one over a half turn (the cosine is negative then).
    const QPointF middle = (a + b) / 2;
    const QPointF direction = (b - a) / chord;
    const QPointF left(-direction.y(), direction.x());
    const double offset = radius * std::cos(qAbs(angle) / 2) * (bulge > 0 ? 1 : -1);
    const QPointF center = middle + left * offset;
    const double start = std::atan2(a.y() - center.y(), a.x() - center.x());
    const int segments = arcSegments(radius, angle, tolerance);
    for (int i = 1; i <= segments; ++i)
    {
        const double t = start + angle * i / segments;
        out.append(i == segments ? b : center + QPointF(std::cos(t), std::sin(t)) * radius);
    }
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief The point of the (rational) B-spline at parameter @p t (de Boor). */
QPointF deBoor(int degree, const QVector<double> &knots, const QVector<QPointF> &points, const QVector<double> &weights,
               double t)
{
    const int n = static_cast<int>(points.size());
    int span = degree;
    while (span < n - 1 && t >= knots.at(span + 1))
    {
        ++span;
    }

    QVector<double> x(degree + 1);
    QVector<double> y(degree + 1);
    QVector<double> w(degree + 1);
    for (int j = 0; j <= degree; ++j)
    {
        const int index = qBound(0, j + span - degree, n - 1);
        const double weight = weights.size() == n ? weights.at(index) : 1.0;
        x[j] = points.at(index).x() * weight;
        y[j] = points.at(index).y() * weight;
        w[j] = weight;
    }
    for (int r = 1; r <= degree; ++r)
    {
        for (int j = degree; j >= r; --j)
        {
            const int i = j + span - degree;
            const double denominator = knots.at(i + degree - r + 1) - knots.at(i);
            const double alpha = denominator > 1e-12 ? (t - knots.at(i)) / denominator : 0.0;
            x[j] = (1 - alpha) * x.at(j - 1) + alpha * x.at(j);
            y[j] = (1 - alpha) * y.at(j - 1) + alpha * y.at(j);
            w[j] = (1 - alpha) * w.at(j - 1) + alpha * w.at(j);
        }
    }
    const double weight = qAbs(w.at(degree)) > 1e-12 ? w.at(degree) : 1.0;
    return QPointF(x.at(degree) / weight, y.at(degree) / weight);
}

//---------------------------------------------------------------------------------------------------------------------
QVector<Group> tokenize(const QString &text)
{
    QVector<Group> groups;
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("\r\n|\r|\n")));
    groups.reserve(static_cast<int>(lines.size() / 2));
    for (int i = 0; i + 1 < lines.size(); i += 2)
    {
        bool ok = false;
        const int code = lines.at(i).trimmed().toInt(&ok);
        if (!ok)
        {
            // A stray empty line: resynchronise on the next line.
            if (lines.at(i).trimmed().isEmpty())
            {
                --i;
                continue;
            }
            break;
        }
        groups.append({code, lines.at(i + 1)});
    }
    return groups;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief entityFromGroups builds one entity from its groups (everything up to the next group 0). */
DxfEntity entityFromGroups(const QString &type, const QVector<Group> &groups)
{
    DxfEntity entity;
    entity.type = type;
    double extrusionZ = 1;
    int flags = 0;
    QPointF first;
    QPointF second;
    QString mtextStart;

    for (const Group &group : groups)
    {
        const double number = (group.code >= 10 && group.code < 60) || (group.code >= 210 && group.code < 240)
                                  ? toDouble(group.value)
                                  : 0.0;
        switch (group.code)
        {
            case 8:
                entity.layer = group.value.trimmed();
                break;
            case 1:
                entity.text = group.value;
                break;
            case 2:
                if (type == QLatin1String("INSERT"))
                {
                    entity.text = group.value.trimmed();
                }
                break;
            case 3:
                if (type == QLatin1String("MTEXT"))
                {
                    mtextStart += group.value;
                }
                break;
            case 10:
                if (type == QLatin1String("LWPOLYLINE") || type == QLatin1String("SPLINE"))
                {
                    entity.points.append(QPointF(number, 0));
                    entity.bulges.append(0);
                }
                else
                {
                    first.setX(number);
                }
                break;
            case 20:
                if ((type == QLatin1String("LWPOLYLINE") || type == QLatin1String("SPLINE")) && !entity.points.isEmpty())
                {
                    entity.points.last().setY(number);
                }
                else
                {
                    first.setY(number);
                }
                break;
            case 11:
                if (type == QLatin1String("SPLINE"))
                {
                    entity.fitPoints.append(QPointF(number, 0));
                }
                else
                {
                    second.setX(number);
                }
                break;
            case 21:
                if (type == QLatin1String("SPLINE") && !entity.fitPoints.isEmpty())
                {
                    entity.fitPoints.last().setY(number);
                }
                else
                {
                    second.setY(number);
                }
                break;
            case 40:
                if (type == QLatin1String("SPLINE"))
                {
                    entity.knots.append(number);
                }
                else if (type == QLatin1String("ELLIPSE"))
                {
                    entity.ratio = number;
                }
                else
                {
                    entity.radius = number;
                }
                break;
            case 41:
                if (type == QLatin1String("SPLINE"))
                {
                    entity.weights.append(number);
                }
                else if (type == QLatin1String("ELLIPSE"))
                {
                    entity.startAngle = number;
                }
                else if (type == QLatin1String("INSERT"))
                {
                    entity.scale.setX(number);
                }
                break;
            case 42:
                if (type == QLatin1String("LWPOLYLINE") && !entity.bulges.isEmpty())
                {
                    entity.bulges.last() = number;
                }
                else if (type == QLatin1String("ELLIPSE"))
                {
                    entity.endAngle = number;
                }
                else if (type == QLatin1String("INSERT"))
                {
                    entity.scale.setY(number);
                }
                else if (type == QLatin1String("VERTEX"))
                {
                    entity.bulges = {number};
                }
                break;
            case 50:
                if (type == QLatin1String("ARC"))
                {
                    entity.startAngle = number;
                }
                else
                {
                    entity.rotation = number;
                }
                break;
            case 51:
                if (type == QLatin1String("ARC"))
                {
                    entity.endAngle = number;
                }
                break;
            case 70:
                flags = group.value.trimmed().toInt();
                break;
            case 71:
                if (type == QLatin1String("SPLINE"))
                {
                    entity.degree = qMax(1, group.value.trimmed().toInt());
                }
                break;
            case 230:
                extrusionZ = number;
                break;
            default:
                break;
        }
    }

    if (type == QLatin1String("LINE"))
    {
        entity.points = {first, second};
    }
    else if (type == QLatin1String("POINT") || type == QLatin1String("TEXT") || type == QLatin1String("MTEXT")
             || type == QLatin1String("INSERT") || type == QLatin1String("CIRCLE") || type == QLatin1String("ARC")
             || type == QLatin1String("VERTEX"))
    {
        entity.points = {first};
    }
    else if (type == QLatin1String("ELLIPSE"))
    {
        entity.points = {first};
        entity.majorAxis = second;
        if (qFuzzyIsNull(entity.endAngle - 360) && groups.size() > 0)
        {
            entity.endAngle = 2 * M_PI; // no end parameter: the whole ellipse
        }
    }

    if (type == QLatin1String("LWPOLYLINE") || type == QLatin1String("POLYLINE") || type == QLatin1String("SPLINE"))
    {
        entity.closed = (flags & 1) != 0;
    }
    if (type == QLatin1String("POLYLINE"))
    {
        entity.degree = flags; // kept for the vertices: spline fit, 3D mesh... (see readEntities)
    }
    if (type == QLatin1String("VERTEX"))
    {
        entity.degree = flags;
    }
    if (type == QLatin1String("MTEXT"))
    {
        entity.text = plainMText(mtextStart + entity.text);
    }
    entity.text = decodeText(entity.text);

    // Entities drawn with the extrusion direction pointing down are mirrored in X.
    if (extrusionZ < 0 && type != QLatin1String("LINE") && type != QLatin1String("POINT")
        && type != QLatin1String("SPLINE") && type != QLatin1String("ELLIPSE"))
    {
        for (QPointF &point : entity.points)
        {
            point.setX(-point.x());
        }
        for (double &bulge : entity.bulges)
        {
            bulge = -bulge;
        }
        if (type == QLatin1String("ARC"))
        {
            const double start = entity.startAngle;
            entity.startAngle = 180 - entity.endAngle;
            entity.endAngle = 180 - start;
        }
        entity.rotation = 180 - entity.rotation;
    }
    return entity;
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief readEntities reads the entities from @p index up to a group 0 named one of @p stops; returns where it stopped.
 */
int readEntities(const QVector<Group> &groups, int index, const QStringList &stops, QVector<DxfEntity> &out)
{
    while (index < groups.size())
    {
        if (groups.at(index).code != 0)
        {
            ++index;
            continue;
        }
        const QString type = groups.at(index).value.trimmed().toUpper();
        if (stops.contains(type))
        {
            return index;
        }

        int end = index + 1;
        while (end < groups.size() && groups.at(end).code != 0)
        {
            ++end;
        }
        DxfEntity entity = entityFromGroups(type, groups.mid(index + 1, end - index - 1));
        index = end;

        static const QStringList kept{QStringLiteral("LINE"),   QStringLiteral("POLYLINE"), QStringLiteral("LWPOLYLINE"),
                                      QStringLiteral("POINT"),  QStringLiteral("TEXT"),     QStringLiteral("MTEXT"),
                                      QStringLiteral("CIRCLE"), QStringLiteral("ARC"),      QStringLiteral("ELLIPSE"),
                                      QStringLiteral("SPLINE"), QStringLiteral("INSERT")};
        if (type == QLatin1String("POLYLINE"))
        {
            // The vertices follow, up to SEQEND.
            const int polylineFlags = entity.degree;
            entity.degree = 3;
            entity.points.clear();
            entity.bulges.clear();
            while (index < groups.size() && groups.at(index).code == 0
                   && groups.at(index).value.trimmed().toUpper() == QLatin1String("VERTEX"))
            {
                int vertexEnd = index + 1;
                while (vertexEnd < groups.size() && groups.at(vertexEnd).code != 0)
                {
                    ++vertexEnd;
                }
                const DxfEntity vertex = entityFromGroups(QStringLiteral("VERTEX"),
                                                          groups.mid(index + 1, vertexEnd - index - 1));
                index = vertexEnd;
                // Spline fit polylines also list their frame's control points: skip those.
                const bool controlPoint = (vertex.degree & 16) != 0;
                if (!controlPoint && !vertex.points.isEmpty())
                {
                    entity.points.append(vertex.points.constFirst());
                    entity.bulges.append(vertex.bulges.isEmpty() ? 0.0 : vertex.bulges.constFirst());
                }
            }
            if (index < groups.size() && groups.at(index).value.trimmed().toUpper() == QLatin1String("SEQEND"))
            {
                ++index;
            }
            // Polyface and polygon meshes are 3D: not pattern geometry.
            if ((polylineFlags & (16 | 64)) != 0)
            {
                continue;
            }
        }
        if (kept.contains(type))
        {
            out.append(entity);
        }
    }
    return index;
}

} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
bool DxfEntity::isCurve() const
{
    static const QStringList curves{QStringLiteral("LINE"),   QStringLiteral("POLYLINE"), QStringLiteral("LWPOLYLINE"),
                                    QStringLiteral("CIRCLE"), QStringLiteral("ARC"),      QStringLiteral("ELLIPSE"),
                                    QStringLiteral("SPLINE")};
    return curves.contains(type);
}

//---------------------------------------------------------------------------------------------------------------------
QPolygonF DxfEntity::flatten(double tolerance) const
{
    QPolygonF out;
    if (type == QLatin1String("LINE"))
    {
        out << points.value(0) << points.value(1);
    }
    else if (type == QLatin1String("POLYLINE") || type == QLatin1String("LWPOLYLINE"))
    {
        if (points.isEmpty())
        {
            return out;
        }
        out.append(points.constFirst());
        const int count = static_cast<int>(points.size());
        const int segments = closed ? count : count - 1;
        for (int i = 0; i < segments; ++i)
        {
            bulgeArc(out, points.at(i), points.at((i + 1) % count), bulges.value(i), tolerance);
        }
        if (closed && out.size() > 1)
        {
            out.removeLast(); // back at the first point
        }
    }
    else if (type == QLatin1String("CIRCLE") || type == QLatin1String("ARC"))
    {
        const QPointF center = points.value(0);
        double start = 0;
        double sweep = 2 * M_PI;
        if (type == QLatin1String("ARC"))
        {
            start = qDegreesToRadians(startAngle);
            sweep = qDegreesToRadians(endAngle) - start;
            while (sweep <= 0)
            {
                sweep += 2 * M_PI;
            }
        }
        const int segments = qMax(8, arcSegments(radius, sweep, tolerance));
        const int last = type == QLatin1String("CIRCLE") ? segments - 1 : segments;
        for (int i = 0; i <= last; ++i)
        {
            const double t = start + sweep * i / segments;
            out.append(center + QPointF(std::cos(t), std::sin(t)) * radius);
        }
    }
    else if (type == QLatin1String("ELLIPSE"))
    {
        const QPointF center = points.value(0);
        const QPointF minor = QPointF(-majorAxis.y(), majorAxis.x()) * ratio;
        double sweep = endAngle - startAngle;
        while (sweep <= 0)
        {
            sweep += 2 * M_PI;
        }
        const double major = QLineF(QPointF(), majorAxis).length();
        const int segments = qMax(16, arcSegments(major, sweep, tolerance));
        const bool full = qAbs(sweep - 2 * M_PI) < 1e-9;
        for (int i = 0; i <= (full ? segments - 1 : segments); ++i)
        {
            const double t = startAngle + sweep * i / segments;
            out.append(center + majorAxis * std::cos(t) + minor * std::sin(t));
        }
    }
    else if (type == QLatin1String("SPLINE"))
    {
        const int n = static_cast<int>(points.size());
        if (n < 2)
        {
            return QPolygonF(fitPoints);
        }
        const int p = qMin(degree, n - 1);
        QVector<double> u = knots;
        if (u.size() != n + p + 1)
        {
            // No usable knots: clamped and uniform.
            u.clear();
            for (int i = 0; i < n + p + 1; ++i)
            {
                u.append(qBound(0, i - p, n - p));
            }
        }
        const double start = u.at(p);
        const double end = u.at(n);
        const int samples = qBound(16, n * 12, 4000);
        for (int i = 0; i <= samples; ++i)
        {
            const double t = i == samples ? end - 1e-12 * qMax(1.0, qAbs(end)) : start + (end - start) * i / samples;
            out.append(deBoor(p, u, points, weights, t));
        }
        if (closed && out.size() > 2 && QLineF(out.constFirst(), out.constLast()).length() < tolerance)
        {
            out.removeLast();
        }
    }
    return out;
}

//---------------------------------------------------------------------------------------------------------------------
const DxfBlock *DxfDocument::block(const QString &name) const
{
    for (const DxfBlock &b : blocks)
    {
        if (b.name.compare(name, Qt::CaseInsensitive) == 0)
        {
            return &b;
        }
    }
    return nullptr;
}

//---------------------------------------------------------------------------------------------------------------------
bool DxfReader::read(const QString &fileName, DxfDocument &document)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        m_error = tr("Could not open %1: %2").arg(fileName, file.errorString());
        return false;
    }
    return read(&file, document);
}

//---------------------------------------------------------------------------------------------------------------------
bool DxfReader::read(QIODevice *device, DxfDocument &document)
{
    if (device == nullptr)
    {
        m_error = tr("No file.");
        return false;
    }
    return parse(device->readAll(), document);
}

//---------------------------------------------------------------------------------------------------------------------
bool DxfReader::parse(const QByteArray &data, DxfDocument &document)
{
    document = DxfDocument();
    m_error.clear();

    if (data.startsWith("AutoCAD Binary DXF"))
    {
        m_error = tr("This is a binary DXF file. Save it as an ASCII (text) DXF and import that.");
        return false;
    }

    // Since AutoCAD 2007 DXF files are UTF-8; older ones use a Windows code page, most often 1252.
    QString text = QString::fromUtf8(data);
    if (text.contains(QChar::ReplacementCharacter))
    {
        text = QString::fromLatin1(data);
    }

    const QVector<Group> groups = tokenize(text);
    bool foundSection = false;
    int i = 0;
    while (i < groups.size())
    {
        const Group &group = groups.at(i);
        if (group.code != 0 || group.value.trimmed().toUpper() != QLatin1String("SECTION") || i + 1 >= groups.size())
        {
            ++i;
            continue;
        }
        foundSection = true;
        const QString section = groups.at(i + 1).value.trimmed().toUpper();
        i += 2;

        if (section == QLatin1String("HEADER"))
        {
            QString variable;
            while (i < groups.size() && !(groups.at(i).code == 0 && groups.at(i).value.trimmed() == QLatin1String("ENDSEC")))
            {
                if (groups.at(i).code == 9)
                {
                    variable = groups.at(i).value.trimmed().toUpper();
                }
                else if (!variable.isEmpty() && !document.header.contains(variable))
                {
                    document.header.insert(variable, groups.at(i).value.trimmed());
                }
                ++i;
            }
        }
        else if (section == QLatin1String("BLOCKS"))
        {
            while (i < groups.size() && !(groups.at(i).code == 0 && groups.at(i).value.trimmed() == QLatin1String("ENDSEC")))
            {
                if (groups.at(i).code == 0 && groups.at(i).value.trimmed().toUpper() == QLatin1String("BLOCK"))
                {
                    DxfBlock block;
                    ++i;
                    QPointF base;
                    while (i < groups.size() && groups.at(i).code != 0)
                    {
                        if (groups.at(i).code == 2)
                        {
                            block.name = groups.at(i).value.trimmed();
                        }
                        else if (groups.at(i).code == 10)
                        {
                            base.setX(toDouble(groups.at(i).value));
                        }
                        else if (groups.at(i).code == 20)
                        {
                            base.setY(toDouble(groups.at(i).value));
                        }
                        ++i;
                    }
                    block.base = base;
                    i = readEntities(groups, i, {QStringLiteral("ENDBLK"), QStringLiteral("ENDSEC")}, block.entities);
                    document.blocks.append(block);
                }
                else
                {
                    ++i;
                }
            }
        }
        else if (section == QLatin1String("ENTITIES"))
        {
            i = readEntities(groups, i, {QStringLiteral("ENDSEC")}, document.entities);
        }
    }

    if (!foundSection)
    {
        m_error = tr("This is not a DXF file, or it is empty.");
        return false;
    }
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
QString DxfReader::errorString() const
{
    return m_error;
}

} // namespace Seamly3D
