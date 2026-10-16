/******************************************************************************
 *   @file   svgreader.cpp
 **  @author Seamly2D project
 **  @date   16 10, 2026
 **
 **  @brief  Reads the drawing of an SVG file into the same form as a DXF drawing.
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

#include "svgreader.h"

#include <QDomDocument>
#include <QFile>
#include <QHash>
#include <QIODevice>
#include <QRegularExpression>
#include <QTransform>
#include <QtMath>

namespace Seamly3D
{

namespace
{

const double millimetersPerPixel = 25.4 / 96.0;
const double flatness = 0.01; // millimeters: how far flattened curves may stray

//---------------------------------------------------------------------------------------------------------------------
/** @brief A length with its unit, in millimeters; @p physical tells whether it had a real unit (not px or none). */
double lengthInMm(const QString &text, bool *physical = nullptr, bool *ok = nullptr)
{
    static const QRegularExpression length(QStringLiteral("^\\s*([-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]+)?)\\s*([a-z%]*)\\s*$"));
    const QRegularExpressionMatch match = length.match(text);
    if (ok != nullptr)
    {
        *ok = match.hasMatch();
    }
    if (!match.hasMatch())
    {
        return 0;
    }
    const double value = match.captured(1).toDouble();
    const QString unit = match.captured(2).toLower();
    static const QHash<QString, double> units{{QStringLiteral("mm"), 1.0},        {QStringLiteral("cm"), 10.0},
                                              {QStringLiteral("in"), 25.4},       {QStringLiteral("pt"), 25.4 / 72},
                                              {QStringLiteral("pc"), 25.4 / 6},   {QStringLiteral("m"), 1000.0},
                                              {QStringLiteral("q"), 0.25}};
    if (physical != nullptr)
    {
        *physical = units.contains(unit);
    }
    return value * units.value(unit, millimetersPerPixel);
}

//---------------------------------------------------------------------------------------------------------------------
double number(const QDomElement &element, const QString &name, double fallback = 0)
{
    if (!element.hasAttribute(name))
    {
        return fallback;
    }
    // Lengths inside the drawing are in user units; a unit on them is rare, read it as pixels.
    static const QRegularExpression value(QStringLiteral("[-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]+)?"));
    const QRegularExpressionMatch match = value.match(element.attribute(name));
    return match.hasMatch() ? match.captured(0).toDouble() : fallback;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<double> numbers(const QString &text)
{
    static const QRegularExpression value(QStringLiteral("[-+]?(?:[0-9]*\\.[0-9]+|[0-9]+\\.?)(?:[eE][-+]?[0-9]+)?"));
    QVector<double> result;
    QRegularExpressionMatchIterator it = value.globalMatch(text);
    while (it.hasNext())
    {
        result.append(it.next().captured(0).toDouble());
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief The SVG transform list: matrix, translate, scale, rotate, skewX, skewY. */
QTransform parseTransform(const QString &text)
{
    QTransform result;
    static const QRegularExpression item(QStringLiteral("(matrix|translate|scale|rotate|skewX|skewY)\\s*\\(([^)]*)\\)"));
    QRegularExpressionMatchIterator it = item.globalMatch(text);
    QVector<QTransform> list;
    while (it.hasNext())
    {
        const QRegularExpressionMatch match = it.next();
        const QString name = match.captured(1);
        const QVector<double> v = numbers(match.captured(2));
        QTransform t;
        if (name == QLatin1String("matrix") && v.size() >= 6)
        {
            t = QTransform(v.at(0), v.at(1), v.at(2), v.at(3), v.at(4), v.at(5));
        }
        else if (name == QLatin1String("translate") && !v.isEmpty())
        {
            t.translate(v.at(0), v.value(1, 0));
        }
        else if (name == QLatin1String("scale") && !v.isEmpty())
        {
            t.scale(v.at(0), v.value(1, v.at(0)));
        }
        else if (name == QLatin1String("rotate") && !v.isEmpty())
        {
            if (v.size() >= 3)
            {
                t.translate(v.at(1), v.at(2));
                t.rotate(v.at(0));
                t.translate(-v.at(1), -v.at(2));
            }
            else
            {
                t.rotate(v.at(0));
            }
        }
        else if (name == QLatin1String("skewX") && !v.isEmpty())
        {
            t.shear(std::tan(qDegreesToRadians(v.at(0))), 0);
        }
        else if (name == QLatin1String("skewY") && !v.isEmpty())
        {
            t.shear(0, std::tan(qDegreesToRadians(v.at(0))));
        }
        list.append(t);
    }
    // The list applies right to left: the last transform is nearest to the element.
    for (int i = static_cast<int>(list.size()) - 1; i >= 0; --i)
    {
        result = result * list.at(i);
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief The value of a presentation property, from the style attribute or the attribute itself. */
QString property(const QDomElement &element, const QString &name)
{
    const QString style = element.attribute(QStringLiteral("style"));
    if (!style.isEmpty())
    {
        const QRegularExpression item(QStringLiteral("(?:^|;)\\s*") + QRegularExpression::escape(name)
                                      + QStringLiteral("\\s*:\\s*([^;]+)"));
        const QRegularExpressionMatch match = item.match(style);
        if (match.hasMatch())
        {
            return match.captured(1).trimmed();
        }
    }
    return element.attribute(name).trimmed();
}

//---------------------------------------------------------------------------------------------------------------------
void cubic(QPolygonF &out, const QPointF &p0, const QPointF &p1, const QPointF &p2, const QPointF &p3,
           double tolerance, int depth = 0)
{
    // Flat enough: the control points are close to the chord.
    const QLineF chord(p0, p3);
    const double length = chord.length();
    const auto distance = [&chord, length](const QPointF &p)
    {
        if (length < 1e-12)
        {
            return QLineF(chord.p1(), p).length();
        }
        return qAbs((chord.dx() * (chord.p1().y() - p.y()) - chord.dy() * (chord.p1().x() - p.x())) / length);
    };
    if (depth >= 16 || qMax(distance(p1), distance(p2)) <= tolerance)
    {
        out.append(p3);
        return;
    }
    const QPointF a = (p0 + p1) / 2;
    const QPointF b = (p1 + p2) / 2;
    const QPointF c = (p2 + p3) / 2;
    const QPointF d = (a + b) / 2;
    const QPointF e = (b + c) / 2;
    const QPointF m = (d + e) / 2;
    cubic(out, p0, a, d, m, tolerance, depth + 1);
    cubic(out, m, e, c, p3, tolerance, depth + 1);
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief An SVG elliptical arc, from its end points (SVG 1.1, appendix F.6.5), without the first point. */
void arc(QPolygonF &out, const QPointF &from, double rx, double ry, double rotation, bool large, bool sweep,
         const QPointF &to, double tolerance)
{
    rx = qAbs(rx);
    ry = qAbs(ry);
    if (rx < 1e-12 || ry < 1e-12 || QLineF(from, to).length() < 1e-12)
    {
        out.append(to);
        return;
    }
    const double phi = qDegreesToRadians(rotation);
    const double c = std::cos(phi);
    const double s = std::sin(phi);
    const double dx = (from.x() - to.x()) / 2;
    const double dy = (from.y() - to.y()) / 2;
    const double x1 = c * dx + s * dy;
    const double y1 = -s * dx + c * dy;
    const double lambda = (x1 * x1) / (rx * rx) + (y1 * y1) / (ry * ry);
    if (lambda > 1)
    {
        rx *= std::sqrt(lambda);
        ry *= std::sqrt(lambda);
    }
    const double numerator = rx * rx * ry * ry - rx * rx * y1 * y1 - ry * ry * x1 * x1;
    const double denominator = rx * rx * y1 * y1 + ry * ry * x1 * x1;
    double factor = denominator > 0 ? std::sqrt(qMax(0.0, numerator / denominator)) : 0;
    if (large == sweep)
    {
        factor = -factor;
    }
    const double cx1 = factor * rx * y1 / ry;
    const double cy1 = -factor * ry * x1 / rx;
    const QPointF center(c * cx1 - s * cy1 + (from.x() + to.x()) / 2, s * cx1 + c * cy1 + (from.y() + to.y()) / 2);
    const auto angle = [](double ux, double uy, double vx, double vy)
    { return std::atan2(ux * vy - uy * vx, ux * vx + uy * vy); };
    const double start = angle(1, 0, (x1 - cx1) / rx, (y1 - cy1) / ry);
    double delta = angle((x1 - cx1) / rx, (y1 - cy1) / ry, (-x1 - cx1) / rx, (-y1 - cy1) / ry);
    if (!sweep && delta > 0)
    {
        delta -= 2 * M_PI;
    }
    else if (sweep && delta < 0)
    {
        delta += 2 * M_PI;
    }
    const double radius = qMax(rx, ry);
    const double step = radius > tolerance ? 2 * std::acos(qBound(-1.0, 1 - tolerance / radius, 1.0)) : M_PI / 8;
    const int segments = qBound(2, qCeil(qAbs(delta) / qMax(step, 1e-3)), 720);
    for (int i = 1; i <= segments; ++i)
    {
        const double t = start + delta * i / segments;
        const double ex = rx * std::cos(t);
        const double ey = ry * std::sin(t);
        out.append(i == segments ? to : center + QPointF(c * ex - s * ey, s * ex + c * ey));
    }
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief Reads the numbers and flags of path data, one at a time. */
class PathScanner
{
public:
    explicit PathScanner(const QString &data)
        : m_data(data)
    {}

    void skip()
    {
        while (m_pos < m_data.size() && (m_data.at(m_pos).isSpace() || m_data.at(m_pos) == QLatin1Char(',')))
        {
            ++m_pos;
        }
    }
    bool atEnd()
    {
        skip();
        return m_pos >= m_data.size();
    }
    bool atNumber()
    {
        skip();
        if (m_pos >= m_data.size())
        {
            return false;
        }
        const QChar ch = m_data.at(m_pos);
        return ch.isDigit() || ch == QLatin1Char('-') || ch == QLatin1Char('+') || ch == QLatin1Char('.');
    }
    QChar command()
    {
        skip();
        return m_pos < m_data.size() ? m_data.at(m_pos++) : QChar();
    }
    double number()
    {
        skip();
        const int start = m_pos;
        if (m_pos < m_data.size() && (m_data.at(m_pos) == QLatin1Char('-') || m_data.at(m_pos) == QLatin1Char('+')))
        {
            ++m_pos;
        }
        bool dot = false;
        while (m_pos < m_data.size()
               && (m_data.at(m_pos).isDigit() || (!dot && m_data.at(m_pos) == QLatin1Char('.'))))
        {
            dot = dot || m_data.at(m_pos) == QLatin1Char('.');
            ++m_pos;
        }
        if (m_pos < m_data.size() && (m_data.at(m_pos) == QLatin1Char('e') || m_data.at(m_pos) == QLatin1Char('E')))
        {
            ++m_pos;
            if (m_pos < m_data.size() && (m_data.at(m_pos) == QLatin1Char('-') || m_data.at(m_pos) == QLatin1Char('+')))
            {
                ++m_pos;
            }
            while (m_pos < m_data.size() && m_data.at(m_pos).isDigit())
            {
                ++m_pos;
            }
        }
        return m_data.mid(start, m_pos - start).toDouble();
    }
    bool flag()
    {
        skip();
        // Arc flags may be written without separators: "a5 5 0 01 10 10".
        return m_pos < m_data.size() && m_data.at(m_pos++) == QLatin1Char('1');
    }

private:
    const QString &m_data;
    int            m_pos{0};
};

//---------------------------------------------------------------------------------------------------------------------
/** @brief The sub paths of SVG path data, flattened: points, and whether each is closed. */
QVector<QPair<QPolygonF, bool>> parsePath(const QString &data, double tolerance)
{
    QVector<QPair<QPolygonF, bool>> result;
    PathScanner scan(data);
    QPolygonF current;
    QPointF point;
    QPointF start;
    QPointF lastControl;
    QChar previous;
    QChar command;

    const auto finish = [&](bool closed)
    {
        if (current.size() > 1)
        {
            result.append(qMakePair(current, closed));
        }
        current.clear();
    };

    while (!scan.atEnd())
    {
        if (!scan.atNumber())
        {
            command = scan.command();
        }
        else if (command.isNull())
        {
            break; // numbers before any command
        }
        const bool relative = command.isLower();
        const QPointF origin = relative ? point : QPointF();
        switch (command.toUpper().unicode())
        {
            case 'M':
            {
                finish(false);
                const double x = scan.number();
                const double y = scan.number();
                point = origin + QPointF(x, y);
                start = point;
                current.append(point);
                command = relative ? QLatin1Char('l') : QLatin1Char('L'); // further pairs are lines
                break;
            }
            case 'L':
            {
                const double x = scan.number();
                const double y = scan.number();
                point = origin + QPointF(x, y);
                current.append(point);
                break;
            }
            case 'H':
                point.setX((relative ? point.x() : 0) + scan.number());
                current.append(point);
                break;
            case 'V':
                point.setY((relative ? point.y() : 0) + scan.number());
                current.append(point);
                break;
            case 'C':
            {
                const double x1 = scan.number();
                const double y1 = scan.number();
                const double x2 = scan.number();
                const double y2 = scan.number();
                const double x = scan.number();
                const double y = scan.number();
                const QPointF c1 = origin + QPointF(x1, y1);
                const QPointF c2 = origin + QPointF(x2, y2);
                const QPointF to = origin + QPointF(x, y);
                cubic(current, point, c1, c2, to, tolerance);
                lastControl = c2;
                point = to;
                break;
            }
            case 'S':
            {
                const bool smooth = QStringLiteral("CcSs").contains(previous);
                const QPointF c1 = smooth ? point * 2 - lastControl : point;
                const double x2 = scan.number();
                const double y2 = scan.number();
                const double x = scan.number();
                const double y = scan.number();
                const QPointF c2 = origin + QPointF(x2, y2);
                const QPointF to = origin + QPointF(x, y);
                cubic(current, point, c1, c2, to, tolerance);
                lastControl = c2;
                point = to;
                break;
            }
            case 'Q':
            case 'T':
            {
                QPointF control;
                if (command.toUpper() == QLatin1Char('Q'))
                {
                    const double x1 = scan.number();
                    const double y1 = scan.number();
                    control = origin + QPointF(x1, y1);
                }
                else
                {
                    control = QStringLiteral("QqTt").contains(previous) ? point * 2 - lastControl : point;
                }
                const double x = scan.number();
                const double y = scan.number();
                const QPointF to = origin + QPointF(x, y);
                cubic(current, point, point + (control - point) * (2.0 / 3), to + (control - to) * (2.0 / 3), to,
                      tolerance);
                lastControl = control;
                point = to;
                break;
            }
            case 'A':
            {
                const double rx = scan.number();
                const double ry = scan.number();
                const double rotation = scan.number();
                const bool large = scan.flag();
                const bool sweep = scan.flag();
                const double x = scan.number();
                const double y = scan.number();
                const QPointF to = origin + QPointF(x, y);
                arc(current, point, rx, ry, rotation, large, sweep, to, tolerance);
                point = to;
                break;
            }
            case 'Z':
                if (!current.isEmpty() && QLineF(current.constFirst(), current.constLast()).length() < 1e-9)
                {
                    current.removeLast();
                }
                finish(true);
                point = start;
                current.append(point); // a path may go on from where it closed
                break;
            default:
                return result; // unknown command: stop here, as SVG renderers do
        }
        previous = command;
    }
    finish(false);
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief Walks the drawing, collecting polylines and texts. */
class Walker
{
public:
    Walker(DxfDocument &document, const QHash<QString, QDomElement> &ids)
        : m_document(document), m_ids(ids)
    {}

    void walk(const QDomElement &element, const QTransform &transform, int depth);

private:
    void add(const QPolygonF &points, bool closed, const QDomElement &element, const QTransform &transform);
    void addText(const QDomElement &element, const QTransform &transform);
    double tolerance(const QTransform &transform) const
    {
        const double s = std::sqrt(qAbs(transform.determinant()));
        return flatness / qMax(1e-12, s);
    }

    DxfDocument                      &m_document;
    const QHash<QString, QDomElement> &m_ids;
};

//---------------------------------------------------------------------------------------------------------------------
void Walker::add(const QPolygonF &points, bool closed, const QDomElement &element, const QTransform &transform)
{
    if (points.size() < 2)
    {
        return;
    }
    DxfEntity entity;
    entity.type = QStringLiteral("LWPOLYLINE");
    const QString cls = element.attribute(QStringLiteral("class")).simplified();
    entity.layer = !cls.isEmpty() ? cls : element.attribute(QStringLiteral("id"), QStringLiteral("0"));
    for (const QPointF &p : points)
    {
        const QPointF mapped = transform.map(p);
        entity.points.append(QPointF(mapped.x(), -mapped.y())); // Y up, as in DXF
    }
    entity.bulges = QVector<double>(entity.points.size(), 0.0);
    entity.closed = closed;
    m_document.entities.append(entity);
}

//---------------------------------------------------------------------------------------------------------------------
void Walker::addText(const QDomElement &element, const QTransform &transform)
{
    // Lines of text are often tspans with positions of their own.
    QPointF position(number(element, QStringLiteral("x")), number(element, QStringLiteral("y")));
    QPointF line = position; // where the current line of text is
    QString text;
    bool lines = false;
    for (QDomNode node = element.firstChild(); !node.isNull(); node = node.nextSibling())
    {
        if (node.isText())
        {
            text += node.toText().data();
            continue;
        }
        const QDomElement span = node.toElement();
        if (span.tagName() != QLatin1String("tspan"))
        {
            continue;
        }
        if (span.hasAttribute(QStringLiteral("x")) || span.hasAttribute(QStringLiteral("y"))
            || span.hasAttribute(QStringLiteral("dx")) || span.hasAttribute(QStringLiteral("dy")))
        {
            lines = true;
            line = QPointF(number(span, QStringLiteral("x"), line.x()) + number(span, QStringLiteral("dx")),
                           number(span, QStringLiteral("y"), line.y()) + number(span, QStringLiteral("dy")));
            DxfEntity entity;
            entity.type = QStringLiteral("TEXT");
            entity.text = span.text().simplified();
            const QPointF p = transform.map(line);
            entity.points = {QPointF(p.x(), -p.y())};
            if (!entity.text.isEmpty())
            {
                m_document.entities.append(entity);
            }
        }
        else
        {
            text += span.text();
        }
    }
    text = text.simplified();
    if (!text.isEmpty() || !lines)
    {
        DxfEntity entity;
        entity.type = QStringLiteral("TEXT");
        entity.text = text;
        const QPointF p = transform.map(position);
        entity.points = {QPointF(p.x(), -p.y())};
        if (!text.isEmpty())
        {
            m_document.entities.append(entity);
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
void Walker::walk(const QDomElement &element, const QTransform &parent, int depth)
{
    if (depth > 64)
    {
        return;
    }
    static const QStringList hidden{QStringLiteral("defs"),     QStringLiteral("clipPath"),
                                    QStringLiteral("mask"),     QStringLiteral("pattern"),
                                    QStringLiteral("marker"),   QStringLiteral("symbol"),
                                    QStringLiteral("metadata"), QStringLiteral("title"),
                                    QStringLiteral("desc"),     QStringLiteral("style"),
                                    QStringLiteral("script"),   QStringLiteral("linearGradient"),
                                    QStringLiteral("radialGradient"), QStringLiteral("filter"),
                                    QStringLiteral("image"),    QStringLiteral("foreignObject")};
    const QString tag = element.localName().isEmpty() ? element.tagName() : element.localName();
    if (hidden.contains(tag) || property(element, QStringLiteral("display")) == QLatin1String("none")
        || property(element, QStringLiteral("visibility")) == QLatin1String("hidden"))
    {
        return;
    }

    const QTransform transform = parseTransform(element.attribute(QStringLiteral("transform"))) * parent;
    const double tol = tolerance(transform);

    if (tag == QLatin1String("g") || tag == QLatin1String("a") || tag == QLatin1String("switch")
        || tag == QLatin1String("svg"))
    {
        QTransform inner = transform;
        if (tag == QLatin1String("svg") && depth > 0)
        {
            // A nested drawing: placed at x, y, its viewBox fitted to its width and height.
            QTransform place;
            place.translate(number(element, QStringLiteral("x")), number(element, QStringLiteral("y")));
            const QVector<double> box = numbers(element.attribute(QStringLiteral("viewBox")));
            if (box.size() == 4 && box.at(2) > 0 && box.at(3) > 0)
            {
                const double sx = number(element, QStringLiteral("width"), box.at(2)) / box.at(2);
                const double sy = number(element, QStringLiteral("height"), box.at(3)) / box.at(3);
                QTransform fit;
                fit.translate(-box.at(0), -box.at(1));
                fit = fit * QTransform::fromScale(qMin(sx, sy), qMin(sx, sy));
                place = fit * place;
            }
            inner = place * transform;
        }
        for (QDomElement child = element.firstChildElement(); !child.isNull(); child = child.nextSiblingElement())
        {
            walk(child, inner, depth + 1);
        }
    }
    else if (tag == QLatin1String("use"))
    {
        QString reference = element.attribute(QStringLiteral("href"));
        if (reference.isEmpty())
        {
            reference = element.attributeNS(QStringLiteral("http://www.w3.org/1999/xlink"), QStringLiteral("href"));
        }
        if (reference.isEmpty())
        {
            reference = element.attribute(QStringLiteral("xlink:href"));
        }
        const QDomElement target = m_ids.value(reference.mid(1));
        if (reference.startsWith(QLatin1Char('#')) && !target.isNull())
        {
            const QTransform placed = QTransform::fromTranslate(number(element, QStringLiteral("x")),
                                                                number(element, QStringLiteral("y")))
                                      * transform;
            // Drawn as if it were here, even when it sits in <defs>; a symbol draws its content.
            const QString targetTag = target.localName().isEmpty() ? target.tagName() : target.localName();
            if (targetTag == QLatin1String("symbol"))
            {
                for (QDomElement child = target.firstChildElement(); !child.isNull();
                     child = child.nextSiblingElement())
                {
                    walk(child, placed, depth + 1);
                }
            }
            else
            {
                walk(target, placed, depth + 1);
            }
        }
    }
    else if (tag == QLatin1String("path"))
    {
        for (const auto &sub : parsePath(element.attribute(QStringLiteral("d")), tol))
        {
            add(sub.first, sub.second, element, transform);
        }
    }
    else if (tag == QLatin1String("line"))
    {
        add(QPolygonF({QPointF(number(element, QStringLiteral("x1")), number(element, QStringLiteral("y1"))),
                       QPointF(number(element, QStringLiteral("x2")), number(element, QStringLiteral("y2")))}),
            false, element, transform);
    }
    else if (tag == QLatin1String("polyline") || tag == QLatin1String("polygon"))
    {
        const QVector<double> v = numbers(element.attribute(QStringLiteral("points")));
        QPolygonF points;
        for (int i = 0; i + 1 < v.size(); i += 2)
        {
            points.append(QPointF(v.at(i), v.at(i + 1)));
        }
        add(points, tag == QLatin1String("polygon"), element, transform);
    }
    else if (tag == QLatin1String("rect"))
    {
        const QRectF rect(number(element, QStringLiteral("x")), number(element, QStringLiteral("y")),
                          number(element, QStringLiteral("width")), number(element, QStringLiteral("height")));
        if (rect.width() > 0 && rect.height() > 0)
        {
            add(QPolygonF({rect.topLeft(), rect.topRight(), rect.bottomRight(), rect.bottomLeft()}), true, element,
                transform);
        }
    }
    else if (tag == QLatin1String("circle") || tag == QLatin1String("ellipse"))
    {
        const QPointF center(number(element, QStringLiteral("cx")), number(element, QStringLiteral("cy")));
        const double rx = tag == QLatin1String("circle") ? number(element, QStringLiteral("r"))
                                                         : number(element, QStringLiteral("rx"));
        const double ry = tag == QLatin1String("circle") ? rx : number(element, QStringLiteral("ry"), rx);
        if (rx > 0 && ry > 0)
        {
            const double radius = qMax(rx, ry);
            const double step = radius > tol ? 2 * std::acos(qBound(-1.0, 1 - tol / radius, 1.0)) : M_PI / 8;
            const int segments = qBound(12, qCeil(2 * M_PI / qMax(step, 1e-3)), 720);
            QPolygonF points;
            for (int i = 0; i < segments; ++i)
            {
                const double t = 2 * M_PI * i / segments;
                points.append(center + QPointF(rx * std::cos(t), ry * std::sin(t)));
            }
            add(points, true, element, transform);
        }
    }
    else if (tag == QLatin1String("text"))
    {
        addText(element, transform);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void collectIds(const QDomElement &element, QHash<QString, QDomElement> &ids)
{
    if (element.hasAttribute(QStringLiteral("id")))
    {
        ids.insert(element.attribute(QStringLiteral("id")), element);
    }
    for (QDomElement child = element.firstChildElement(); !child.isNull(); child = child.nextSiblingElement())
    {
        collectIds(child, ids);
    }
}

} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
bool SvgReader::isSvg(const QByteArray &start)
{
    // DXF is lines of group codes and values; SVG is XML.
    QByteArray head = start.left(4096).trimmed();
    if (head.startsWith("\xEF\xBB\xBF"))
    {
        head = head.mid(3).trimmed(); // UTF-8 byte order mark
    }
    return head.startsWith('<');
}

//---------------------------------------------------------------------------------------------------------------------
bool SvgReader::read(const QString &fileName, DxfDocument &document)
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
bool SvgReader::read(QIODevice *device, DxfDocument &document)
{
    if (device == nullptr)
    {
        m_error = tr("No file.");
        return false;
    }
    return parse(device->readAll(), document);
}

//---------------------------------------------------------------------------------------------------------------------
bool SvgReader::parse(const QByteArray &data, DxfDocument &document)
{
    document = DxfDocument();
    m_error.clear();

    QDomDocument dom;
    QString message;
    int line = 0;
    if (!dom.setContent(data, true, &message, &line))
    {
        m_error = tr("This is not a readable SVG file: %1 (line %2).").arg(message).arg(line);
        return false;
    }
    const QDomElement root = dom.documentElement();
    if (root.localName() != QLatin1String("svg") && root.tagName() != QLatin1String("svg"))
    {
        m_error = tr("This is not an SVG drawing.");
        return false;
    }

    // Scale: the drawing's size in real units over its viewBox. Without units, 96 pixels per inch.
    const QVector<double> box = numbers(root.attribute(QStringLiteral("viewBox")));
    bool physical = false;
    bool hasWidth = false;
    const double widthMm = lengthInMm(root.attribute(QStringLiteral("width")), &physical, &hasWidth);
    double scale = millimetersPerPixel;
    QTransform view;
    if (box.size() == 4 && box.at(2) > 0)
    {
        view.translate(-box.at(0), -box.at(1));
        if (hasWidth && widthMm > 0)
        {
            scale = widthMm / box.at(2);
        }
    }
    view = view * QTransform::fromScale(scale, scale);

    QHash<QString, QDomElement> ids;
    collectIds(root, ids);
    Walker walker(document, ids);
    walker.walk(root, view, 0);

    document.header.insert(QStringLiteral("$INSUNITS"), QStringLiteral("4")); // millimeters
    if (document.entities.isEmpty())
    {
        m_error = tr("The SVG drawing has no lines.");
        return false;
    }
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
QString SvgReader::errorString() const
{
    return m_error;
}

} // namespace Seamly3D
