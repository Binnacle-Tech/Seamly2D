/******************************************************************************
 *   @file   dxfpatternconverter.cpp
 **  @author Seamly2D project
 **  @date   16 10, 2026
 **
 **  @brief  Converts the pieces of a DXF pattern into a parametric Seamly2D pattern (.val).
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

#include "dxfpatternconverter.h"

#include <QBuffer>
#include <QFile>
#include <QSaveFile>
#include <QSet>
#include <QXmlStreamWriter>
#include <QtMath>

#include <algorithm>
#include <limits>

namespace Seamly3D
{

namespace
{

const double pageWidth = 150.0;  // centimeters: pieces are laid out in rows this wide
const double pieceGap = 5.0;     // centimeters between pieces

//---------------------------------------------------------------------------------------------------------------------
/** @brief A number for the file: up to 4 decimals, no trailing zeros. */
QString number(double value)
{
    QString text = QString::number(value, 'f', 4);
    if (text.contains(QLatin1Char('.')))
    {
        while (text.endsWith(QLatin1Char('0')))
        {
            text.chop(1);
        }
        if (text.endsWith(QLatin1Char('.')))
        {
            text.chop(1);
        }
    }
    if (text == QLatin1String("-0"))
    {
        text = QStringLiteral("0");
    }
    return text;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief Seamly2D's angle of a direction: degrees counterclockwise on screen (Y down), 0 to 360. */
QString angleOf(const QPointF &vector)
{
    double angle = QLineF(QPointF(), vector).angle();
    if (angle >= 359.99995)
    {
        angle = 0;
    }
    return number(angle);
}

//---------------------------------------------------------------------------------------------------------------------
double signedArea(const QPolygonF &polygon)
{
    double area = 0;
    for (int i = 0; i < polygon.size(); ++i)
    {
        const QPointF &a = polygon.at(i);
        const QPointF &b = polygon.at((i + 1) % polygon.size());
        area += a.x() * b.y() - b.x() * a.y();
    }
    return area / 2;
}

//---------------------------------------------------------------------------------------------------------------------
double distanceToSegment(const QPointF &p, const QPointF &a, const QPointF &b)
{
    const QPointF ab = b - a;
    const double l2 = QPointF::dotProduct(ab, ab);
    const double t = l2 > 0 ? qBound(0.0, QPointF::dotProduct(p - a, ab) / l2, 1.0) : 0.0;
    return QLineF(p, a + ab * t).length();
}

//---------------------------------------------------------------------------------------------------------------------
double distanceToOutline(const QPointF &p, const QPolygonF &outline)
{
    double best = std::numeric_limits<double>::max();
    for (int i = 0; i < outline.size(); ++i)
    {
        best = qMin(best, distanceToSegment(p, outline.at(i), outline.at((i + 1) % outline.size())));
    }
    return best;
}

//---------------------------------------------------------------------------------------------------------------------
QPointF segmentPoint(const FittedPoint &a, const FittedPoint &b, double t)
{
    if (a.lineAfter)
    {
        return a.position + (b.position - a.position) * t;
    }
    const QPointF p1 = a.position + a.handleOut;
    const QPointF p2 = b.position + b.handleIn;
    const double s = 1 - t;
    return a.position * (s * s * s) + p1 * (3 * s * s * t) + p2 * (3 * s * t * t) + b.position * (t * t * t);
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief The letters of the @p index-th piece: A to Z, then AA, AB... */
QString pieceLetters(int index)
{
    QString letters;
    int i = index;
    do
    {
        letters.prepend(QChar(QLatin1Char('A').unicode() + i % 26));
        i = i / 26 - 1;
    } while (i >= 0);
    return letters;
}

//---------------------------------------------------------------------------------------------------------------------
double pixelsPerUnit(const QString &unit)
{
    // Seamly2D's scene works at 96 pixels per inch.
    return 96.0 / 2.54 * DxfPattern::toCentimeters(unit);
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief The curves of a fitted path, as runs of curved segments between corners and notches (spline paths).
 */
struct CurveRun
{
    QVector<int> points; // indices in the fitted path, the last may be 0 again for a closed path
};

QVector<CurveRun> curveRuns(const FittedPath &path)
{
    QVector<CurveRun> runs;
    const int n = static_cast<int>(path.points.size());
    const int segments = path.closed ? n : n - 1;
    CurveRun current;
    for (int i = 0; i < segments; ++i)
    {
        const FittedPoint &point = path.points.at(i);
        if (point.lineAfter)
        {
            if (current.points.size() > 1)
            {
                runs.append(current);
            }
            current = CurveRun();
            continue;
        }
        if (!current.points.isEmpty() && (point.corner || point.notch))
        {
            runs.append(current); // a curve ends at a corner or a notch
            current = CurveRun();
        }
        if (current.points.isEmpty())
        {
            current.points.append(i);
        }
        current.points.append((i + 1) % n);
    }
    if (current.points.size() > 1)
    {
        runs.append(current);
    }

    // A spline path may not start and end at the same point: split such a loop in two.
    QVector<CurveRun> result;
    for (const CurveRun &run : qAsConst(runs))
    {
        if (run.points.size() > 2 && run.points.constFirst() == run.points.constLast())
        {
            const int middle = static_cast<int>(run.points.size()) / 2;
            result.append({run.points.mid(0, middle + 1)});
            result.append({run.points.mid(middle)});
        }
        else
        {
            result.append(run);
        }
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief Writes the draft block of one piece, using and advancing @p nextId. */
class BlockWriter
{
public:
    BlockWriter(QXmlStreamWriter &xml, const DxfImportOptions &options, quint32 &nextId)
        : m_xml(xml), m_options(options), m_nextId(nextId),
          m_cm(1.0 / DxfPattern::toCentimeters(options.targetUnit)), m_pixels(pixelsPerUnit(options.targetUnit))
    {}

    void write(const PreparedPiece &piece, const QString &letters, const QString &blockName);

private:
    struct PathIds
    {
        QVector<quint32>  point{};     // calculation point of each fitted point
        QVector<CurveRun> runs{};
        QVector<quint32>  spline{};    // calculation spline path of each run
        QVector<bool>     node{};      // the fitted point is a node of the piece (not inside a curve)
    };

    quint32 id() { return m_nextId++; }
    void    writePoint(quint32 pointId, const QString &name, const QPointF &position, quint32 basePointId,
                       const QPointF &basePosition, bool line);
    PathIds writePath(const FittedPath &path, const QString &prefix, quint32 basePointId, const QPointF &basePosition,
                      bool reuseFirst);
    void    writeSplines(const FittedPath &path, PathIds &ids);
    void    writeModeling(const PathIds &ids, QVector<quint32> &modelPoint, QVector<quint32> &modelSpline);

    QXmlStreamWriter       &m_xml;
    const DxfImportOptions &m_options;
    quint32                &m_nextId;
    double                  m_cm;     // target units per centimeter
    double                  m_pixels; // scene pixels per target unit
};

//---------------------------------------------------------------------------------------------------------------------
void BlockWriter::writePoint(quint32 pointId, const QString &name, const QPointF &position, quint32 basePointId,
                             const QPointF &basePosition, bool line)
{
    m_xml.writeStartElement(QStringLiteral("point"));
    m_xml.writeAttribute(QStringLiteral("id"), QString::number(pointId));
    m_xml.writeAttribute(QStringLiteral("name"), name);
    m_xml.writeAttribute(QStringLiteral("mx"), number(0.13 * m_cm));
    m_xml.writeAttribute(QStringLiteral("my"), number(0.26 * m_cm));
    m_xml.writeAttribute(QStringLiteral("showPointName"), QStringLiteral("true"));
    if (basePointId == 0)
    {
        m_xml.writeAttribute(QStringLiteral("type"), QStringLiteral("single"));
        m_xml.writeAttribute(QStringLiteral("x"), number(position.x()));
        m_xml.writeAttribute(QStringLiteral("y"), number(position.y()));
    }
    else
    {
        // A length and an angle from the base point: numbers to edit, or to replace with formulas.
        m_xml.writeAttribute(QStringLiteral("type"), QStringLiteral("endLine"));
        m_xml.writeAttribute(QStringLiteral("basePoint"), QString::number(basePointId));
        m_xml.writeAttribute(QStringLiteral("length"), number(QLineF(basePosition, position).length()));
        m_xml.writeAttribute(QStringLiteral("angle"), angleOf(position - basePosition));
        m_xml.writeAttribute(QStringLiteral("lineType"), line ? QStringLiteral("solidLine") : QStringLiteral("none"));
        m_xml.writeAttribute(QStringLiteral("lineWeight"), QStringLiteral("0.35"));
        m_xml.writeAttribute(QStringLiteral("lineColor"), QStringLiteral("black"));
    }
    m_xml.writeEndElement();
}

//---------------------------------------------------------------------------------------------------------------------
BlockWriter::PathIds BlockWriter::writePath(const FittedPath &path, const QString &prefix, quint32 basePointId,
                                            const QPointF &basePosition, bool reuseFirst)
{
    PathIds ids;
    const int n = static_cast<int>(path.points.size());
    ids.point.resize(n);
    for (int k = 0; k < n; ++k)
    {
        const FittedPoint &point = path.points.at(k);
        if (k == 0 && reuseFirst)
        {
            ids.point[k] = basePointId; // the outline's first point is the piece's base point
            continue;
        }
        ids.point[k] = id();
        quint32 from = basePointId;
        QPointF fromPosition = basePosition;
        bool line = false;
        if (m_options.fromPreviousPoint && k > 0)
        {
            from = ids.point.at(k - 1);
            fromPosition = path.points.at(k - 1).position;
            line = path.points.at(k - 1).lineAfter;
        }
        writePoint(ids.point.at(k), prefix + QString::number(k + 1), point.position, from, fromPosition, line);
    }

    // Straight edges not drawn by the points themselves.
    const int segments = path.closed ? n : n - 1;
    for (int k = 0; k < segments; ++k)
    {
        const bool drawn = m_options.fromPreviousPoint && k + 1 < n;
        if (!path.points.at(k).lineAfter || drawn)
        {
            continue;
        }
        m_xml.writeStartElement(QStringLiteral("line"));
        m_xml.writeAttribute(QStringLiteral("id"), QString::number(id()));
        m_xml.writeAttribute(QStringLiteral("firstPoint"), QString::number(ids.point.at(k)));
        m_xml.writeAttribute(QStringLiteral("secondPoint"), QString::number(ids.point.at((k + 1) % n)));
        m_xml.writeAttribute(QStringLiteral("lineType"), QStringLiteral("solidLine"));
        m_xml.writeAttribute(QStringLiteral("lineWeight"), QStringLiteral("0.35"));
        m_xml.writeAttribute(QStringLiteral("lineColor"), QStringLiteral("black"));
        m_xml.writeEndElement();
    }

    writeSplines(path, ids);
    return ids;
}

//---------------------------------------------------------------------------------------------------------------------
void BlockWriter::writeSplines(const FittedPath &path, PathIds &ids)
{
    const int n = static_cast<int>(path.points.size());
    ids.runs = curveRuns(path);
    ids.node = QVector<bool>(n, true);
    for (const CurveRun &run : qAsConst(ids.runs))
    {
        for (int k = 1; k + 1 < run.points.size(); ++k)
        {
            ids.node[run.points.at(k)] = false; // inside the curve
        }

        const quint32 splineId = id();
        ids.spline.append(splineId);
        m_xml.writeStartElement(QStringLiteral("spline"));
        m_xml.writeAttribute(QStringLiteral("id"), QString::number(splineId));
        m_xml.writeAttribute(QStringLiteral("type"), QStringLiteral("pathInteractive"));
        m_xml.writeAttribute(QStringLiteral("color"), QStringLiteral("black"));
        m_xml.writeAttribute(QStringLiteral("penStyle"), QStringLiteral("solidLine"));
        m_xml.writeAttribute(QStringLiteral("lineWeight"), QStringLiteral("0.35"));
        for (int k = 0; k < run.points.size(); ++k)
        {
            const FittedPoint &point = path.points.at(run.points.at(k));
            // Handles: out toward the next point, in from the previous one. The ends of the path use only one, the
            // other mirrors it.
            QPointF in = point.handleIn;
            QPointF out = point.handleOut;
            if (k == 0)
            {
                in = -out;
            }
            if (k + 1 == run.points.size())
            {
                out = -in;
            }
            const double minimum = 1e-4;
            if (QLineF(QPointF(), in).length() < minimum)
            {
                in = QLineF(QPointF(), -out).unitVector().p2() * minimum;
            }
            if (QLineF(QPointF(), out).length() < minimum)
            {
                out = QLineF(QPointF(), -in).unitVector().p2() * minimum;
            }

            m_xml.writeStartElement(QStringLiteral("pathPoint"));
            m_xml.writeAttribute(QStringLiteral("pSpline"), QString::number(ids.point.at(run.points.at(k))));
            m_xml.writeAttribute(QStringLiteral("angle1"), angleOf(in));
            m_xml.writeAttribute(QStringLiteral("length1"), number(qMax(minimum, QLineF(QPointF(), in).length())));
            m_xml.writeAttribute(QStringLiteral("angle2"), angleOf(out));
            m_xml.writeAttribute(QStringLiteral("length2"), number(qMax(minimum, QLineF(QPointF(), out).length())));
            m_xml.writeEndElement();
        }
        m_xml.writeEndElement();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void BlockWriter::writeModeling(const PathIds &ids, QVector<quint32> &modelPoint, QVector<quint32> &modelSpline)
{
    modelPoint = QVector<quint32>(ids.point.size(), 0);
    for (int k = 0; k < ids.point.size(); ++k)
    {
        if (!ids.node.at(k))
        {
            continue;
        }
        modelPoint[k] = id();
        m_xml.writeStartElement(QStringLiteral("point"));
        m_xml.writeAttribute(QStringLiteral("id"), QString::number(modelPoint.at(k)));
        m_xml.writeAttribute(QStringLiteral("type"), QStringLiteral("modeling"));
        m_xml.writeAttribute(QStringLiteral("idObject"), QString::number(ids.point.at(k)));
        m_xml.writeAttribute(QStringLiteral("mx"), number(0.13 * m_cm));
        m_xml.writeAttribute(QStringLiteral("my"), number(0.26 * m_cm));
        m_xml.writeAttribute(QStringLiteral("showPointName"), QStringLiteral("true"));
        m_xml.writeEndElement();
    }
    modelSpline.clear();
    for (const quint32 splineId : ids.spline)
    {
        modelSpline.append(id());
        m_xml.writeStartElement(QStringLiteral("spline"));
        m_xml.writeAttribute(QStringLiteral("id"), QString::number(modelSpline.constLast()));
        m_xml.writeAttribute(QStringLiteral("type"), QStringLiteral("modelingPath"));
        m_xml.writeAttribute(QStringLiteral("idObject"), QString::number(splineId));
        m_xml.writeEndElement();
    }
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief Writes the nodes of a path: its points, and its curves between them. */
void writeNodes(QXmlStreamWriter &xml, const FittedPath &path, const QVector<CurveRun> &runs,
                const QVector<quint32> &modelPoint, const QVector<quint32> &modelSpline,
                const std::function<void(int)> &pointAttributes)
{
    const int n = static_cast<int>(path.points.size());
    QHash<int, int> runFrom; // fitted point -> curve starting there
    for (int r = 0; r < runs.size(); ++r)
    {
        runFrom.insert(runs.at(r).points.constFirst(), r);
    }
    for (int k = 0; k < n; ++k)
    {
        if (modelPoint.at(k) != 0)
        {
            xml.writeStartElement(QStringLiteral("node"));
            xml.writeAttribute(QStringLiteral("type"), QStringLiteral("NodePoint"));
            xml.writeAttribute(QStringLiteral("idObject"), QString::number(modelPoint.at(k)));
            if (pointAttributes)
            {
                pointAttributes(k);
            }
            xml.writeEndElement();
        }
        if (runFrom.contains(k))
        {
            xml.writeStartElement(QStringLiteral("node"));
            xml.writeAttribute(QStringLiteral("type"), QStringLiteral("NodeSplinePath"));
            xml.writeAttribute(QStringLiteral("idObject"), QString::number(modelSpline.at(runFrom.value(k))));
            xml.writeAttribute(QStringLiteral("reverse"), QStringLiteral("0"));
            xml.writeEndElement();
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
void BlockWriter::write(const PreparedPiece &piece, const QString &letters, const QString &blockName)
{
    m_xml.writeStartElement(QStringLiteral("draftBlock"));
    m_xml.writeAttribute(QStringLiteral("name"), blockName);

    // Calculation: the points, lines and curves.
    m_xml.writeStartElement(QStringLiteral("calculation"));
    const quint32 baseId = id();
    const QPointF basePosition = piece.outline.points.constFirst().position;
    writePoint(baseId, letters + QStringLiteral("1"), basePosition, 0, basePosition, false);
    PathIds outline = writePath(piece.outline, letters, baseId, basePosition, true);

    QVector<PathIds> internals;
    for (int i = 0; i < piece.internalLines.size(); ++i)
    {
        const QString prefix = letters + QStringLiteral("i") + QString::number(i + 1) + QStringLiteral("p");
        internals.append(writePath(piece.internalLines.at(i), prefix, baseId, basePosition, false));
    }
    QVector<quint32> holes;
    for (int i = 0; i < piece.drillHoles.size(); ++i)
    {
        holes.append(id());
        writePoint(holes.constLast(), letters + QStringLiteral("d") + QString::number(i + 1), piece.drillHoles.at(i),
                   baseId, basePosition, false);
    }
    m_xml.writeEndElement(); // calculation

    // Modeling: the copies of the points and curves the piece is made of.
    m_xml.writeStartElement(QStringLiteral("modeling"));
    QVector<quint32> modelPoint;
    QVector<quint32> modelSpline;
    writeModeling(outline, modelPoint, modelSpline);

    QVector<quint32> internalPaths;
    for (int i = 0; i < internals.size(); ++i)
    {
        QVector<quint32> points;
        QVector<quint32> splines;
        writeModeling(internals.at(i), points, splines);
        internalPaths.append(id());
        m_xml.writeStartElement(QStringLiteral("path"));
        m_xml.writeAttribute(QStringLiteral("id"), QString::number(internalPaths.constLast()));
        m_xml.writeAttribute(QStringLiteral("type"), QStringLiteral("2")); // internal path
        m_xml.writeAttribute(QStringLiteral("name"), DxfPatternConverter::tr("Internal line %1").arg(i + 1));
        m_xml.writeAttribute(QStringLiteral("lineColor"), QStringLiteral("black"));
        m_xml.writeAttribute(QStringLiteral("lineType"), QStringLiteral("solidLine"));
        m_xml.writeAttribute(QStringLiteral("lineWeight"), QStringLiteral("0.35"));
        m_xml.writeAttribute(QStringLiteral("cut"), QStringLiteral("false"));
        m_xml.writeStartElement(QStringLiteral("nodes"));
        writeNodes(m_xml, piece.internalLines.at(i), internals.at(i).runs, points, splines, nullptr);
        if (piece.internalLines.at(i).closed && !points.isEmpty() && points.constFirst() != 0)
        {
            // A closed internal line ends where it starts.
            m_xml.writeStartElement(QStringLiteral("node"));
            m_xml.writeAttribute(QStringLiteral("type"), QStringLiteral("NodePoint"));
            m_xml.writeAttribute(QStringLiteral("idObject"), QString::number(points.constFirst()));
            m_xml.writeEndElement();
        }
        m_xml.writeEndElement(); // nodes
        m_xml.writeEndElement(); // path
    }

    QVector<quint32> anchors;
    for (const quint32 hole : qAsConst(holes))
    {
        anchors.append(id());
        m_xml.writeStartElement(QStringLiteral("point"));
        m_xml.writeAttribute(QStringLiteral("id"), QString::number(anchors.constLast()));
        m_xml.writeAttribute(QStringLiteral("type"), QStringLiteral("anchor"));
        m_xml.writeAttribute(QStringLiteral("idObject"), QString::number(hole));
        m_xml.writeEndElement();
    }
    m_xml.writeEndElement(); // modeling

    // The piece.
    const QRectF bounds = piece.outline.painterPath().boundingRect().united(piece.cutLine.boundingRect());
    m_xml.writeStartElement(QStringLiteral("pieces"));
    m_xml.writeStartElement(QStringLiteral("piece"));
    m_xml.writeAttribute(QStringLiteral("id"), QString::number(id()));
    m_xml.writeAttribute(QStringLiteral("version"), QStringLiteral("2"));
    m_xml.writeAttribute(QStringLiteral("name"), piece.name);
    m_xml.writeAttribute(QStringLiteral("mx"), QStringLiteral("0"));
    m_xml.writeAttribute(QStringLiteral("my"), QStringLiteral("0"));
    m_xml.writeAttribute(QStringLiteral("inLayout"), QStringLiteral("true"));
    m_xml.writeAttribute(QStringLiteral("forbidFlipping"), QStringLiteral("false"));
    m_xml.writeAttribute(QStringLiteral("seamAllowance"), piece.seamAllowance ? QStringLiteral("true")
                                                                              : QStringLiteral("false"));
    if (piece.seamAllowanceBuiltIn)
    {
        m_xml.writeAttribute(QStringLiteral("seamAllowanceBuiltIn"), QStringLiteral("true"));
    }
    m_xml.writeAttribute(QStringLiteral("width"), number(piece.width));
    m_xml.writeAttribute(QStringLiteral("united"), QStringLiteral("false"));
    m_xml.writeAttribute(QStringLiteral("hideMainPath"), QStringLiteral("false"));
    m_xml.writeAttribute(QStringLiteral("locked"), QStringLiteral("false"));

    // Label with the name and quantity, in the middle of the piece.
    const double labelWidth = qMin(bounds.width() * 0.5, 8.0 * m_cm);
    const double labelHeight = labelWidth / 2;
    m_xml.writeStartElement(QStringLiteral("data"));
    m_xml.writeAttribute(QStringLiteral("letter"), letters);
    m_xml.writeAttribute(QStringLiteral("annotation"), QString());
    m_xml.writeAttribute(QStringLiteral("orientation"), QString());
    m_xml.writeAttribute(QStringLiteral("rotationWay"), QString());
    m_xml.writeAttribute(QStringLiteral("tilt"), QString());
    m_xml.writeAttribute(QStringLiteral("foldPosition"), QString());
    m_xml.writeAttribute(QStringLiteral("visible"), QStringLiteral("true"));
    m_xml.writeAttribute(QStringLiteral("onFold"), piece.onFold ? QStringLiteral("true") : QStringLiteral("false"));
    m_xml.writeAttribute(QStringLiteral("quantity"), QString::number(qMax(1, piece.quantity)));
    m_xml.writeAttribute(QStringLiteral("fontSize"), QStringLiteral("0"));
    m_xml.writeAttribute(QStringLiteral("mx"), number((bounds.center().x() - labelWidth / 2) * m_pixels));
    m_xml.writeAttribute(QStringLiteral("my"), number((bounds.center().y() - labelHeight / 2) * m_pixels));
    m_xml.writeAttribute(QStringLiteral("width"), number(labelWidth));
    m_xml.writeAttribute(QStringLiteral("height"), number(labelHeight));
    m_xml.writeAttribute(QStringLiteral("rotation"), QStringLiteral("0"));
    m_xml.writeEndElement();

    m_xml.writeStartElement(QStringLiteral("patternInfo"));
    m_xml.writeAttribute(QStringLiteral("visible"), QStringLiteral("false"));
    m_xml.writeAttribute(QStringLiteral("fontSize"), QStringLiteral("0"));
    m_xml.writeAttribute(QStringLiteral("mx"), QStringLiteral("0"));
    m_xml.writeAttribute(QStringLiteral("my"), QStringLiteral("0"));
    m_xml.writeAttribute(QStringLiteral("width"), QStringLiteral("1"));
    m_xml.writeAttribute(QStringLiteral("height"), QStringLiteral("1"));
    m_xml.writeAttribute(QStringLiteral("rotation"), QStringLiteral("0"));
    m_xml.writeEndElement();

    m_xml.writeStartElement(QStringLiteral("grainline"));
    m_xml.writeAttribute(QStringLiteral("visible"), piece.hasGrainline ? QStringLiteral("true") : QStringLiteral("false"));
    if (piece.hasGrainline)
    {
        // Starts at its tail; position in scene pixels, length in the pattern's units.
        m_xml.writeAttribute(QStringLiteral("mx"), number(piece.grainline.p1().x() * m_pixels));
        m_xml.writeAttribute(QStringLiteral("my"), number(piece.grainline.p1().y() * m_pixels));
        m_xml.writeAttribute(QStringLiteral("length"), number(piece.grainline.length()));
        m_xml.writeAttribute(QStringLiteral("rotation"), angleOf(piece.grainline.p2() - piece.grainline.p1()));
    }
    else
    {
        m_xml.writeAttribute(QStringLiteral("mx"), QStringLiteral("0"));
        m_xml.writeAttribute(QStringLiteral("my"), QStringLiteral("0"));
        m_xml.writeAttribute(QStringLiteral("length"), number(10 * m_cm));
        m_xml.writeAttribute(QStringLiteral("rotation"), QStringLiteral("90"));
    }
    m_xml.writeAttribute(QStringLiteral("arrows"), QStringLiteral("0"));
    m_xml.writeEndElement();

    // Seam allowance edge by edge, where it differs from the piece's.
    const int n = static_cast<int>(piece.outline.points.size());
    QVector<double> before(n, -1);
    QVector<double> after(n, -1);
    if (piece.segmentWidths.size() == n)
    {
        for (int k = 0; k < n; ++k)
        {
            after[k] = piece.segmentWidths.at(k);
            before[(k + 1) % n] = piece.segmentWidths.at(k);
        }
        // A curve has one allowance: the one its first edge has, up to its end point.
        for (const CurveRun &run : qAsConst(outline.runs))
        {
            const double width = piece.segmentWidths.at(run.points.constFirst());
            after[run.points.constFirst()] = width;
            before[run.points.constLast()] = width;
        }
    }
    const double same = 0.05 * m_cm;
    m_xml.writeStartElement(QStringLiteral("nodes"));
    writeNodes(m_xml, piece.outline, outline.runs, modelPoint, modelSpline,
               [&](int k)
               {
                   if (before.at(k) >= 0 && qAbs(before.at(k) - piece.width) > same)
                   {
                       m_xml.writeAttribute(QStringLiteral("before"), number(before.at(k)));
                   }
                   if (after.at(k) >= 0 && qAbs(after.at(k) - piece.width) > same)
                   {
                       m_xml.writeAttribute(QStringLiteral("after"), number(after.at(k)));
                   }
                   if (piece.outline.points.at(k).notch)
                   {
                       m_xml.writeAttribute(QStringLiteral("notch"), QStringLiteral("true"));
                       m_xml.writeAttribute(QStringLiteral("notchType"), QStringLiteral("slit"));
                       m_xml.writeAttribute(QStringLiteral("notchSubtype"), QStringLiteral("straightforward"));
                       m_xml.writeAttribute(QStringLiteral("showNotch"), QStringLiteral("true"));
                       m_xml.writeAttribute(QStringLiteral("showSecondNotch"), QStringLiteral("true"));
                   }
               });
    m_xml.writeEndElement(); // nodes

    if (!internalPaths.isEmpty())
    {
        m_xml.writeStartElement(QStringLiteral("iPaths"));
        for (const quint32 path : qAsConst(internalPaths))
        {
            m_xml.writeStartElement(QStringLiteral("record"));
            m_xml.writeAttribute(QStringLiteral("path"), QString::number(path));
            m_xml.writeEndElement();
        }
        m_xml.writeEndElement();
    }
    if (!anchors.isEmpty())
    {
        m_xml.writeStartElement(QStringLiteral("anchors"));
        for (const quint32 anchor : qAsConst(anchors))
        {
            m_xml.writeTextElement(QStringLiteral("record"), QString::number(anchor));
        }
        m_xml.writeEndElement();
    }

    m_xml.writeEndElement(); // piece
    m_xml.writeEndElement(); // pieces
    m_xml.writeEndElement(); // draftBlock
}

} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
QString DxfPatternConverter::formatVersion()
{
    return QStringLiteral("0.7.5");
}

//---------------------------------------------------------------------------------------------------------------------
QVector<PreparedPiece> DxfPatternConverter::prepare(const DxfPattern &pattern, const DxfImportOptions &options)
{
    QVector<PreparedPiece> result;
    const double scale = DxfPattern::toCentimeters(options.sourceUnit) / DxfPattern::toCentimeters(options.targetUnit);
    const double cm = 1.0 / DxfPattern::toCentimeters(options.targetUnit);

    OutlineFitter::Settings settings;
    settings.tolerance = options.curveTolerance * cm;
    settings.cornerAngle = options.cornerAngle;
    settings.straightLength = 2.0 * cm;

    double x = 0;
    double y = 0;
    double rowHeight = 0;

    for (int i = 0; i < pattern.pieces.size(); ++i)
    {
        if (!options.include.isEmpty() && !options.include.value(i, false))
        {
            continue;
        }
        const DxfPiece &source = pattern.pieces.at(i);

        PreparedPiece piece;
        piece.source = i;
        const QString name = options.names.value(i).trimmed();
        piece.name = name.isEmpty() ? source.name : name;
        const int quantity = options.quantities.value(i, 0);
        piece.quantity = quantity > 0 ? quantity : source.quantity;
        piece.onFold = source.hasMirrorLine;

        // Y up in the file, down in the pattern.
        const auto map = [scale](const QPointF &p) { return QPointF(p.x() * scale, -p.y() * scale); };
        const auto mapPolygon = [&map](const QPolygonF &polygon)
        {
            QPolygonF result;
            for (const QPointF &p : polygon)
            {
                result.append(map(p));
            }
            return result;
        };

        QPolygonF seam;
        QPolygonF cut = mapPolygon(source.cutLine);
        bool allowanceFromFile = false;
        switch (options.seamLine)
        {
            case DxfImportOptions::SeamLine::CutLineAddAllowance:
                seam = cut;
                piece.seamAllowance = true;
                piece.width = options.seamAllowance;
                break;
            case DxfImportOptions::SeamLine::CutLineIncludesAllowance:
                seam = cut;
                piece.seamAllowance = true;
                piece.seamAllowanceBuiltIn = true;
                break;
            case DxfImportOptions::SeamLine::Automatic:
            default:
                if (!source.sewLine.isEmpty())
                {
                    seam = mapPolygon(source.sewLine);
                    piece.seamAllowance = true;
                    allowanceFromFile = true;
                }
                else
                {
                    seam = cut;
                    piece.seamAllowance = true;
                    piece.seamAllowanceBuiltIn = true;
                }
                break;
        }
        if (seam.size() < 3)
        {
            continue;
        }

        // Laid out in rows, left to right.
        const QRectF bounds = seam.boundingRect().united(cut.boundingRect());
        if (x > 0 && x + bounds.width() > pageWidth * cm)
        {
            x = 0;
            y += rowHeight + pieceGap * cm;
            rowHeight = 0;
        }
        const QPointF offset(x - bounds.left(), y - bounds.top());
        x += bounds.width() + pieceGap * cm;
        rowHeight = qMax(rowHeight, bounds.height());

        const auto place = [&map, &offset](const QPointF &p) { return map(p) + offset; };
        seam.translate(offset);
        cut.translate(offset);

        // Seamly2D wants the seam line clockwise on screen.
        if (signedArea(seam) < 0)
        {
            std::reverse(seam.begin(), seam.end());
        }

        QVector<QPointF> corners;
        for (const QPointF &p : source.turnPoints)
        {
            corners.append(place(p));
        }
        QVector<QPointF> curvePoints;
        for (const QPointF &p : source.curvePoints)
        {
            curvePoints.append(place(p));
        }
        QVector<QPointF> notches;
        for (const QLineF &notch : source.notches)
        {
            notches.append(place(notch.p1()));
        }
        piece.outline = OutlineFitter::fit(seam, true, settings, corners, notches, curvePoints);
        piece.cutLine = cut;

        for (const QPolygonF &line : source.internalLines)
        {
            QPolygonF placed;
            for (const QPointF &p : line)
            {
                placed.append(place(p));
            }
            const bool closed = placed.size() > 3
                                && QLineF(placed.constFirst(), placed.constLast()).length() <= settings.tolerance;
            if (closed)
            {
                placed.removeLast();
            }
            if (placed.size() >= 2)
            {
                piece.internalLines.append(OutlineFitter::fit(placed, closed, settings));
            }
        }
        for (const QPointF &p : source.drillHoles)
        {
            piece.drillHoles.append(place(p));
        }
        if (source.hasGrainline && source.grainline.length() > 1e-9)
        {
            piece.hasGrainline = true;
            piece.grainline = QLineF(place(source.grainline.p1()), place(source.grainline.p2()));
        }

        // The allowance the file has: how far the cut line is from each edge of the seam line.
        if (allowanceFromFile && !cut.isEmpty())
        {
            const int n = static_cast<int>(piece.outline.points.size());
            QVector<QPair<double, double>> weighted; // width, edge length
            for (int k = 0; k < n; ++k)
            {
                const FittedPoint &a = piece.outline.points.at(k);
                const FittedPoint &b = piece.outline.points.at((k + 1) % n);
                QVector<double> distances;
                for (const double t : {0.2, 0.35, 0.5, 0.65, 0.8})
                {
                    distances.append(distanceToOutline(segmentPoint(a, b, t), cut));
                }
                std::sort(distances.begin(), distances.end());
                const double step = 0.01 * cm; // a tenth of a millimeter
                const double width = qRound(distances.at(distances.size() / 2) / step) * step;
                piece.segmentWidths.append(width);
                weighted.append(qMakePair(width, QLineF(a.position, b.position).length()));
            }
            // The piece's allowance: what most of its outline has.
            std::sort(weighted.begin(), weighted.end());
            double total = 0;
            for (const auto &w : qAsConst(weighted))
            {
                total += w.second;
            }
            double running = 0;
            for (const auto &w : qAsConst(weighted))
            {
                running += w.second;
                if (running >= total / 2)
                {
                    piece.width = w.first;
                    break;
                }
            }
        }
        result.append(piece);
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
QByteArray DxfPatternConverter::write(const QVector<PreparedPiece> &pieces, const DxfImportOptions &options)
{
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);

    QXmlStreamWriter xml(&buffer);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(4);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("pattern"));
    xml.writeComment(QStringLiteral("Pattern created with Seamly2D (https://seamly.io/), converted from DXF."));
    xml.writeTextElement(QStringLiteral("version"), formatVersion());
    xml.writeTextElement(QStringLiteral("unit"), options.targetUnit);
    xml.writeTextElement(QStringLiteral("description"), options.description);
    xml.writeTextElement(QStringLiteral("notes"), QString());
    if (!options.patternName.isEmpty())
    {
        xml.writeTextElement(QStringLiteral("patternName"), options.patternName);
    }
    xml.writeTextElement(QStringLiteral("measurements"), QString());
    xml.writeEmptyElement(QStringLiteral("variables"));

    quint32 nextId = 1;
    QSet<QString> blockNames;
    for (int i = 0; i < pieces.size(); ++i)
    {
        const PreparedPiece &piece = pieces.at(i);
        if (piece.outline.points.size() < 3)
        {
            continue;
        }
        // Draft block names are unique.
        QString blockName = piece.name.trimmed().isEmpty() ? tr("Piece %1").arg(i + 1) : piece.name.trimmed();
        const QString base = blockName;
        for (int copy = 2; blockNames.contains(blockName.toLower()); ++copy)
        {
            blockName = QStringLiteral("%1 (%2)").arg(base).arg(copy);
        }
        blockNames.insert(blockName.toLower());

        BlockWriter writer(xml, options, nextId);
        writer.write(piece, pieceLetters(i), blockName);
    }

    xml.writeEndElement(); // pattern
    xml.writeEndDocument();
    return data;
}

//---------------------------------------------------------------------------------------------------------------------
bool DxfPatternConverter::convert(const DxfPattern &pattern, const DxfImportOptions &options, const QString &fileName,
                                  QString *error)
{
    const QVector<PreparedPiece> pieces = prepare(pattern, options);
    if (pieces.isEmpty())
    {
        if (error != nullptr)
        {
            *error = tr("There are no pieces to convert.");
        }
        return false;
    }

    QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly) || file.write(write(pieces, options)) < 0 || !file.commit())
    {
        if (error != nullptr)
        {
            *error = tr("Could not write %1: %2").arg(fileName, file.errorString());
        }
        return false;
    }
    return true;
}

} // namespace Seamly3D
