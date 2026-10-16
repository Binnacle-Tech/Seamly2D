/******************************************************************************
 *   @file   dxfpattern.cpp
 **  @author Seamly2D project
 **  @date   16 10, 2026
 **
 **  @brief  Pattern pieces found in a DXF drawing: AAMA/ASTM layers, or closed outlines of any DXF.
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

#include "dxfpattern.h"

#include <QRegularExpression>
#include <QTransform>
#include <QSet>
#include <QtMath>

#include <algorithm>
#include <functional>
#include <limits>

namespace Seamly3D
{

namespace
{

//---------------------------------------------------------------------------------------------------------------------
/** @brief Where a block's entities go in the drawing: around the block's base, scaled, rotated, then moved. */
QTransform insertTransform(const QPointF &base, const QPointF &position, const QPointF &scale, double rotation)
{
    QTransform rotate;
    rotate.rotate(rotation); // counterclockwise, Y up
    return QTransform::fromTranslate(-base.x(), -base.y()) * QTransform::fromScale(scale.x(), scale.y()) * rotate
           * QTransform::fromTranslate(position.x(), position.y());
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
    const double length2 = QPointF::dotProduct(ab, ab);
    const double t = length2 > 0 ? qBound(0.0, QPointF::dotProduct(p - a, ab) / length2, 1.0) : 0.0;
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
/** @brief isInside is true when (nearly) all points of @p inner are inside @p outer. */
bool isInside(const QPolygonF &inner, const QPolygonF &outer)
{
    if (inner.isEmpty())
    {
        return false;
    }
    int inside = 0;
    for (const QPointF &point : inner)
    {
        if (outer.containsPoint(point, Qt::OddEvenFill))
        {
            ++inside;
        }
    }
    return inside >= qCeil(inner.size() * 0.9);
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief isOffsetOf is true when @p inner runs at a nearly constant distance inside @p outer: a seam line. */
bool isOffsetOf(const QPolygonF &inner, const QPolygonF &outer)
{
    if (inner.size() < 3)
    {
        return false;
    }
    double sum = 0;
    double sum2 = 0;
    for (const QPointF &point : inner)
    {
        const double d = distanceToOutline(point, outer);
        sum += d;
        sum2 += d * d;
    }
    const double mean = sum / inner.size();
    const double deviation = std::sqrt(qMax(0.0, sum2 / inner.size() - mean * mean));
    const QRectF box = outer.boundingRect();
    const double size = qMax(box.width(), box.height());
    return mean > 0 && mean < size * 0.08 && deviation < mean * 0.35 + size * 0.002
           && qAbs(signedArea(inner)) > qAbs(signedArea(outer)) * 0.6;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief Removes repeated points and the closing point. */
QPolygonF cleaned(const QPolygonF &polygon, double tolerance)
{
    QPolygonF result;
    for (const QPointF &point : polygon)
    {
        if (result.isEmpty() || QLineF(result.constLast(), point).length() > tolerance * 0.01)
        {
            result.append(point);
        }
    }
    while (result.size() > 2 && QLineF(result.constFirst(), result.constLast()).length() <= tolerance * 0.01)
    {
        result.removeLast();
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief chainLoops joins open curves end to end into closed outlines. Curves that do not close stay in @p leftover.
 */
QVector<QPolygonF> chainLoops(QVector<QPolygonF> open, double gap, QVector<QPolygonF> *leftover)
{
    QVector<QPolygonF> loops;
    while (!open.isEmpty())
    {
        QPolygonF chain = open.takeFirst();
        bool grown = true;
        while (grown && QLineF(chain.constFirst(), chain.constLast()).length() > gap)
        {
            grown = false;
            for (int i = 0; i < open.size(); ++i)
            {
                QPolygonF next = open.at(i);
                if (QLineF(chain.constLast(), next.constFirst()).length() <= gap)
                {
                    // as it is
                }
                else if (QLineF(chain.constLast(), next.constLast()).length() <= gap)
                {
                    std::reverse(next.begin(), next.end());
                }
                else if (QLineF(chain.constFirst(), next.constLast()).length() <= gap)
                {
                    std::reverse(chain.begin(), chain.end());
                    std::reverse(next.begin(), next.end());
                }
                else if (QLineF(chain.constFirst(), next.constFirst()).length() <= gap)
                {
                    std::reverse(chain.begin(), chain.end());
                }
                else
                {
                    continue;
                }
                next.removeFirst();
                chain += next;
                open.removeAt(i);
                grown = true;
                break;
            }
        }
        if (chain.size() > 2 && QLineF(chain.constFirst(), chain.constLast()).length() <= gap)
        {
            chain.removeLast();
            loops.append(chain);
        }
        else if (leftover != nullptr)
        {
            leftover->append(chain);
        }
    }
    return loops;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief The AAMA layer number of an entity, or -1. */
int aamaLayer(const DxfEntity &entity)
{
    bool ok = false;
    const int layer = entity.layer.toInt(&ok);
    return ok ? layer : -1;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief Splits "Piece Name: Front" into a lower case key and a value. */
bool labelValue(const QString &text, QString &key, QString &value)
{
    static const QRegularExpression label(QStringLiteral("^\\s*([A-Za-z][A-Za-z _/-]*?)\\s*:\\s*(.*?)\\s*$"));
    const QRegularExpressionMatch match = label.match(text);
    if (!match.hasMatch())
    {
        return false;
    }
    key = match.captured(1).toLower().simplified();
    value = match.captured(2);
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief Units from an AAMA "UNITS:" text value. */
QString unitFromName(const QString &value)
{
    const QString v = value.trimmed().toUpper();
    if (v.startsWith(QLatin1String("ENGLISH")) || v.startsWith(QLatin1String("INCH")) || v.startsWith(QLatin1String("IMPERIAL")))
    {
        return QStringLiteral("inch");
    }
    if (v.startsWith(QLatin1String("METRIC")) || v.startsWith(QLatin1String("MM")) || v.startsWith(QLatin1String("MILLIM")))
    {
        return QStringLiteral("mm");
    }
    if (v.startsWith(QLatin1String("CM")) || v.startsWith(QLatin1String("CENTIM")))
    {
        return QStringLiteral("cm");
    }
    return QString();
}

//---------------------------------------------------------------------------------------------------------------------
void collectTexts(const QVector<DxfEntity> &entities, QStringList &texts)
{
    for (const DxfEntity &entity : entities)
    {
        if (entity.type == QLatin1String("TEXT") || entity.type == QLatin1String("MTEXT"))
        {
            texts += entity.text.split(QLatin1Char('\n'));
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief The largest closed outline among @p loops, or an empty one. */
QPolygonF largest(const QVector<QPolygonF> &loops)
{
    QPolygonF best;
    double bestArea = 0;
    for (const QPolygonF &loop : loops)
    {
        const double area = qAbs(signedArea(loop));
        if (area > bestArea)
        {
            bestArea = area;
            best = loop;
        }
    }
    return best;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief Flattened closed and open curves of @p entities on the layers accepted by @p onLayer. */
void curves(const QVector<DxfEntity> &entities, const QTransform &transform, double tolerance,
            const std::function<bool(const DxfEntity &)> &onLayer, QVector<QPolygonF> &closed, QVector<QPolygonF> &open)
{
    for (const DxfEntity &entity : entities)
    {
        if (!entity.isCurve() || !onLayer(entity))
        {
            continue;
        }
        const QPolygonF flat = transform.map(entity.flatten(tolerance));
        // Many exporters close a shape by ending where it starts, without marking it closed.
        const bool endsAtStart = flat.size() > 3 && QLineF(flat.constFirst(), flat.constLast()).length() <= tolerance * 5;
        const QPolygonF points = cleaned(flat, tolerance);
        if (points.size() < 2)
        {
            continue;
        }
        const bool isClosed = entity.closed || endsAtStart || entity.type == QLatin1String("CIRCLE")
                              || (entity.type == QLatin1String("ELLIPSE")
                                  && qAbs(entity.endAngle - entity.startAngle - 2 * M_PI) < 1e-6);
        if (isClosed && points.size() > 2)
        {
            closed.append(points);
        }
        else
        {
            open.append(points);
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
void applyLabels(DxfPiece &piece, const QStringList &texts)
{
    for (const QString &text : texts)
    {
        QString key;
        QString value;
        if (!labelValue(text, key, value) || value.isEmpty())
        {
            continue;
        }
        if (key == QLatin1String("piece name") || key == QLatin1String("piece"))
        {
            piece.name = value;
        }
        else if (key == QLatin1String("quantity"))
        {
            piece.quantity = qMax(1, value.toInt());
        }
        else if (key == QLatin1String("size"))
        {
            piece.size = value;
        }
        else if (key == QLatin1String("category"))
        {
            piece.category = value;
        }
        else if (key == QLatin1String("material"))
        {
            piece.material = value;
        }
        else if (key == QLatin1String("annotation"))
        {
            piece.annotation = value;
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Reads what plain text on a piece says, as pattern sites print it: "Cut 2 from main fabric" (quantity),
 * "Cut on fold", and the piece's name (the first text that is not one of those, a number, a date, a version or the
 * site's name).
 */
void applyPlainTexts(DxfPiece &piece, const QStringList &texts)
{
    static const QRegularExpression cut(QStringLiteral("\\bcut\\s+(\\d+)"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression fold(QStringLiteral("\\bfold\\b"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression notName(
        QStringLiteral("\\bcut\\b|\\bfold\\b|grain|seam allowance|freesewing|\\bv?\\d+\\.\\d+|\\b(19|20)\\d\\d\\b|"
                       "^\\W*\\d+\\W*$|https?:|www\\.|\\.com|\\.org"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression letters(QStringLiteral("[^\\W\\d_]"));
    QString name;
    for (const QString &line : texts)
    {
        const QString text = line.simplified();
        const QRegularExpressionMatch quantity = cut.match(text);
        if (quantity.hasMatch() && piece.quantity == 1)
        {
            piece.quantity = qMax(1, quantity.captured(1).toInt());
        }
        if (fold.match(text).hasMatch() && text.contains(QLatin1String("cut"), Qt::CaseInsensitive))
        {
            piece.hasMirrorLine = true;
        }
        QString key;
        QString value;
        if (name.isEmpty() && text.size() <= 40 && text.count(letters) >= 3 && !notName.match(text).hasMatch()
            && !labelValue(text, key, value))
        {
            name = text;
        }
    }
    if (piece.name.isEmpty())
    {
        piece.name = name;
    }
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief The piece drawn by the entities of an AAMA block, placed in the drawing by @p transform. */
DxfPiece aamaPiece(const DxfBlock &block, const QTransform &transform, double tolerance)
{
    DxfPiece piece;
    piece.blockName = block.name;
    piece.name = block.name;

    const auto layerIs = [](int layer) { return [layer](const DxfEntity &e) { return aamaLayer(e) == layer; }; };

    QVector<QPolygonF> closed;
    QVector<QPolygonF> open;
    curves(block.entities, transform, tolerance, layerIs(1), closed, open);
    closed += chainLoops(open, tolerance * 5, nullptr);
    piece.cutLine = largest(closed);
    if (piece.cutLine.isEmpty())
    {
        // Not on layer 1: the largest outline on any layer.
        closed.clear();
        open.clear();
        curves(block.entities, transform, tolerance, [](const DxfEntity &) { return true; }, closed, open);
        closed += chainLoops(open, tolerance * 5, nullptr);
        piece.cutLine = largest(closed);
    }

    closed.clear();
    open.clear();
    curves(block.entities, transform, tolerance, layerIs(14), closed, open);
    closed += chainLoops(open, tolerance * 5, nullptr);
    piece.sewLine = largest(closed);

    closed.clear();
    open.clear();
    curves(block.entities, transform, tolerance,
           [](const DxfEntity &e) { return aamaLayer(e) == 8 || aamaLayer(e) == 11; }, closed, open);
    for (QPolygonF line : qAsConst(closed))
    {
        line.append(line.constFirst()); // closed internal lines end where they start
        piece.internalLines.append(line);
    }
    piece.internalLines += open;

    QStringList texts;
    for (const DxfEntity &entity : block.entities)
    {
        const int layer = aamaLayer(entity);
        if (entity.type == QLatin1String("TEXT") || entity.type == QLatin1String("MTEXT"))
        {
            texts += entity.text.split(QLatin1Char('\n'));
            continue;
        }
        if (entity.points.isEmpty())
        {
            continue;
        }
        const QPointF first = transform.map(entity.points.constFirst());
        if (entity.type == QLatin1String("POINT"))
        {
            switch (layer)
            {
                case 2:
                    piece.turnPoints.append(first);
                    break;
                case 3:
                    piece.curvePoints.append(first);
                    break;
                case 4:
                    piece.notches.append(QLineF(first, first));
                    break;
                case 13:
                    piece.drillHoles.append(first);
                    break;
                default:
                    break;
            }
        }
        else if (entity.type == QLatin1String("CIRCLE") && layer == 13)
        {
            piece.drillHoles.append(first);
        }
        else if (entity.type == QLatin1String("LINE") && entity.points.size() == 2)
        {
            const QLineF line(first, transform.map(entity.points.at(1)));
            if (layer == 4)
            {
                piece.notches.append(line);
            }
            else if (layer == 7 && !piece.hasGrainline)
            {
                piece.grainline = line;
                piece.hasGrainline = true;
            }
            else if (layer == 6 && !piece.hasMirrorLine)
            {
                piece.mirrorLine = line;
                piece.hasMirrorLine = true;
            }
        }
        else if ((entity.type == QLatin1String("POLYLINE") || entity.type == QLatin1String("LWPOLYLINE"))
                 && (layer == 7 || layer == 6) && entity.points.size() >= 2)
        {
            const QLineF line(first, transform.map(entity.points.constLast()));
            if (layer == 7 && !piece.hasGrainline)
            {
                piece.grainline = line;
                piece.hasGrainline = true;
            }
            else if (layer == 6 && !piece.hasMirrorLine)
            {
                piece.mirrorLine = line;
                piece.hasMirrorLine = true;
            }
        }
    }
    applyLabels(piece, texts);
    return piece;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief Every entity of the drawing, with the blocks it inserts expanded, each with where it is placed. */
void expand(const DxfDocument &document, const QVector<DxfEntity> &entities, const QTransform &transform, int depth,
            QVector<QPair<DxfEntity, QTransform>> &out)
{
    for (const DxfEntity &entity : entities)
    {
        if (entity.type == QLatin1String("INSERT"))
        {
            const DxfBlock *block = document.block(entity.text);
            if (block != nullptr && depth < 4)
            {
                const QTransform inner =
                    insertTransform(block->base, entity.points.value(0), entity.scale, entity.rotation);
                expand(document, block->entities, inner * transform, depth + 1, out);
            }
            continue;
        }
        out.append(qMakePair(entity, transform));
    }
}

//---------------------------------------------------------------------------------------------------------------------
double sizeOf(const QPolygonF &polygon)
{
    const QRectF box = polygon.boundingRect();
    return qMax(box.width(), box.height());
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief How round a closed outline is: 1 for a circle, 0.79 for a square, less for anything longer. */
double roundness(const QPolygonF &polygon)
{
    double perimeter = 0;
    for (int i = 0; i < polygon.size(); ++i)
    {
        perimeter += QLineF(polygon.at(i), polygon.at((i + 1) % polygon.size())).length();
    }
    return perimeter > 0 ? 4 * M_PI * qAbs(signedArea(polygon)) / (perimeter * perimeter) : 0;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief The two points of @p line farthest apart. */
QLineF span(const QPolygonF &line)
{
    QLineF best;
    for (int i = 0; i < line.size(); ++i)
    {
        for (int j = i + 1; j < line.size(); ++j)
        {
            if (QLineF(line.at(i), line.at(j)).length() > best.length())
            {
                best = QLineF(line.at(i), line.at(j));
            }
        }
    }
    return best;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief isStraight is true for a line that does not stray from its span by more than @p share of its length. */
bool isStraight(const QPolygonF &line, double share)
{
    const QLineF chord = span(line);
    if (chord.length() <= 0)
    {
        return false;
    }
    for (const QPointF &p : line)
    {
        if (distanceToSegment(p, chord.p1(), chord.p2()) > chord.length() * share)
        {
            return false;
        }
    }
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Sorts out what is drawn inside and around a piece of a plain DXF.
 *
 * Exports from PDF or SVG draw everything as lines: text is letters outlined, notches are small circles on the edge,
 * the grain line is a long straight line, a fold is marked by a bracket along the edge. Small closed outlines are
 * letters and marks: those centered on the edge are notches, isolated round ones are drill holes (buttons, snaps),
 * the rest is left out.
 */
void classifyMarks(DxfPiece &piece, const QVector<QPolygonF> &innerLoops, const QVector<QPolygonF> &innerLines,
                   const QVector<QPolygonF> &marksAround, const QVector<QPointF> &texts, double smallest,
                   double tolerance, int &leftOut)
{
    const double pieceSize = sizeOf(piece.cutLine);
    const double near = pieceSize * 0.06;

    const auto farthestFromEdge = [&piece](const QPolygonF &line)
    {
        double farthest = 0;
        for (const QPointF &p : line)
        {
            farthest = qMax(farthest, distanceToOutline(p, piece.cutLine));
        }
        return farthest;
    };
    // A line lying on the edge is the edge drawn again.
    const auto onEdge = [&farthestFromEdge, pieceSize, tolerance](const QPolygonF &line)
    { return farthestFromEdge(line) <= qMax(pieceSize * 0.003, tolerance * 5); };
    // A line running along the edge, close to it all along, is a fold mark ("cut on fold" bracket).
    const auto foldMark = [&farthestFromEdge, pieceSize, near](const QPolygonF &line)
    { return line.size() >= 3 && span(line).length() >= pieceSize * 0.15 && farthestFromEdge(line) <= near; };
    // The fold is along the bracket's longest straight stretch.
    const auto markFold = [&piece](const QPolygonF &line)
    {
        if (piece.hasMirrorLine)
        {
            return;
        }
        QLineF longest;
        const int n = static_cast<int>(line.size());
        for (int i = 0; i < n; ++i)
        {
            const QLineF edge(line.at(i), line.at((i + 1) % n));
            if (edge.length() > longest.length())
            {
                longest = edge;
            }
        }
        piece.mirrorLine = longest;
        piece.hasMirrorLine = true;
    };

    // Small outlines (as small as the largest piece's letters, or small for this piece) are marks.
    QVector<QPolygonF> small;
    QVector<QPolygonF> large;
    for (const QPolygonF &loop : innerLoops)
    {
        if (onEdge(loop))
        {
            continue;
        }
        if (sizeOf(loop) >= qMax(pieceSize * 0.06, smallest * 1.5))
        {
            large.append(loop);
        }
        else
        {
            small.append(loop);
        }
    }
    for (const QPolygonF &loop : qAsConst(large))
    {
        if (foldMark(loop))
        {
            markFold(loop);
            continue;
        }
        // A frame around text, or a logo: many letters inside (drawn as outlines, or real text).
        int letters = 0;
        for (const QPolygonF &mark : qAsConst(small))
        {
            letters += loop.containsPoint(mark.boundingRect().center(), Qt::OddEvenFill) ? 1 : 0;
        }
        int lines = 0;
        for (const QPointF &text : texts)
        {
            lines += loop.containsPoint(text, Qt::OddEvenFill) ? 1 : 0;
        }
        if (letters >= 5 || lines >= 3)
        {
            ++leftOut;
            continue;
        }
        QPolygonF line = loop;
        line.append(line.constFirst()); // closed internal lines end where they start
        piece.internalLines.append(line);
    }

    // Notches: small marks centered on the edge, inside or around the piece.
    QVector<QPolygonF> candidates = small + marksAround;
    QVector<bool> used(candidates.size(), false);
    for (int i = 0; i < candidates.size(); ++i)
    {
        const QPolygonF &mark = candidates.at(i);
        const double radius = sizeOf(mark) / 2;
        const QPointF center = mark.boundingRect().center();
        // on the cut line, or on the seam line
        const double fromEdge = qMin(distanceToOutline(center, piece.cutLine),
                                     piece.sewLine.isEmpty() ? std::numeric_limits<double>::max()
                                                             : distanceToOutline(center, piece.sewLine));
        if (radius > pieceSize * 0.03 || fromEdge > radius * 1.2 + tolerance)
        {
            continue;
        }
        used[i] = true;
        bool known = false;
        for (const QLineF &notch : qAsConst(piece.notches))
        {
            known = known || QLineF(notch.p1(), center).length() <= radius;
        }
        if (!known)
        {
            piece.notches.append(QLineF(center, center));
        }
    }

    // Drill holes: round marks inside with nothing else close by (letters always have neighbours); rings of circles
    // around the same center are one hole.
    for (int i = 0; i < small.size(); ++i)
    {
        if (used.at(i))
        {
            continue;
        }
        const QPolygonF &mark = small.at(i);
        const double size = sizeOf(mark);
        const QPointF center = mark.boundingRect().center();
        if (roundness(mark) < 0.9 || size < pieceSize * 0.004)
        {
            continue;
        }
        bool isolated = true;
        for (int j = 0; j < small.size() && isolated; ++j)
        {
            const QPointF other = small.at(j).boundingRect().center();
            const double distance = QLineF(center, other).length();
            isolated = j == i || distance < size * 0.2 || distance > size * 2.5;
        }
        if (!isolated)
        {
            continue;
        }
        bool known = false;
        for (const QPointF &hole : qAsConst(piece.drillHoles))
        {
            known = known || QLineF(hole, center).length() < size * 0.2;
        }
        if (!known)
        {
            piece.drillHoles.append(center);
        }
        used[i] = true;
    }
    for (int i = 0; i < small.size(); ++i)
    {
        leftOut += used.at(i) ? 0 : 1;
    }

    // Lines: the grain line is the longest straight one; a bracket from the edge and back marks a fold.
    int grain = -1;
    for (int i = 0; i < innerLines.size(); ++i)
    {
        const QPolygonF &line = innerLines.at(i);
        if (line.size() >= 2 && sizeOf(line) >= pieceSize * 0.2 && isStraight(line, 0.025)
            && (grain < 0 || span(line).length() > span(innerLines.at(grain)).length()))
        {
            grain = i;
        }
    }
    if (grain >= 0 && !piece.hasGrainline)
    {
        piece.grainline = span(innerLines.at(grain));
        piece.hasGrainline = true;
    }
    for (int i = 0; i < innerLines.size(); ++i)
    {
        const QPolygonF &line = innerLines.at(i);
        if (i == grain)
        {
            continue;
        }
        if (onEdge(line))
        {
            continue;
        }
        if (foldMark(line))
        {
            markFold(line);
            continue;
        }
        if (sizeOf(line) < pieceSize * 0.04)
        {
            ++leftOut; // part of a letter, an arrow head...
            continue;
        }
        piece.internalLines.append(line);
    }
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief Pieces of a DXF that is not an AAMA file: every outermost closed outline. */
QVector<DxfPiece> plainPieces(const DxfDocument &document, double tolerance, QStringList &warnings)
{
    QVector<QPair<DxfEntity, QTransform>> all;
    expand(document, document.entities, QTransform(), 0, all);

    QVector<QPolygonF> closed;
    QVector<QPolygonF> open;
    QVector<QPair<QPointF, QString>> texts;
    for (const auto &item : all)
    {
        const DxfEntity &entity = item.first;
        if (entity.isCurve())
        {
            curves({entity}, item.second, tolerance, [](const DxfEntity &) { return true; }, closed, open);
        }
        else if ((entity.type == QLatin1String("TEXT") || entity.type == QLatin1String("MTEXT"))
                 && !entity.points.isEmpty())
        {
            texts.append(qMakePair(item.second.map(entity.points.constFirst()), entity.text));
        }
    }
    QVector<QPolygonF> leftover;
    closed += chainLoops(open, tolerance * 5, &leftover);

    // Largest first: a piece is an outline not inside a larger one, and not tiny next to the largest (letters drawn
    // as outlines, notch marks, logos).
    std::sort(closed.begin(), closed.end(), [](const QPolygonF &a, const QPolygonF &b)
              { return qAbs(signedArea(a)) > qAbs(signedArea(b)); });
    double largest = 0;
    for (const QPolygonF &loop : qAsConst(closed))
    {
        largest = qMax(largest, sizeOf(loop));
    }
    const double smallest = largest * 0.04;

    QVector<DxfPiece> pieces;
    QVector<QVector<QPolygonF>> innerLoops;
    QVector<QPolygonF> marks; // small outlines outside every piece
    QVector<bool> used(closed.size(), false);
    for (int i = 0; i < closed.size(); ++i)
    {
        if (used.at(i))
        {
            continue;
        }
        bool nested = false;
        for (int p = 0; p < pieces.size() && !nested; ++p)
        {
            if (isInside(closed.at(i), pieces.at(p).cutLine))
            {
                nested = true;
                innerLoops[p].append(closed.at(i));
                used[i] = true;
            }
        }
        if (nested)
        {
            continue;
        }
        used[i] = true;
        if (sizeOf(closed.at(i)) < smallest)
        {
            marks.append(closed.at(i));
            continue;
        }
        DxfPiece piece;
        piece.cutLine = closed.at(i);
        pieces.append(piece);
        innerLoops.append(QVector<QPolygonF>());
    }

    int leftOut = 0;
    for (int p = 0; p < pieces.size(); ++p)
    {
        DxfPiece &piece = pieces[p];
        QVector<QPolygonF> loops;
        for (const QPolygonF &loop : qAsConst(innerLoops.at(p)))
        {
            if (piece.sewLine.isEmpty() && isOffsetOf(loop, piece.cutLine))
            {
                piece.sewLine = loop;
            }
            else
            {
                loops.append(loop);
            }
        }
        QVector<QPolygonF> lines;
        for (const QPolygonF &line : qAsConst(leftover))
        {
            if (isInside(line, piece.cutLine))
            {
                lines.append(line);
            }
        }
        QStringList pieceTexts;
        QVector<QPointF> textPositions;
        for (const auto &text : qAsConst(texts))
        {
            if (piece.cutLine.containsPoint(text.first, Qt::OddEvenFill))
            {
                pieceTexts += text.second.split(QLatin1Char('\n'));
                textPositions.append(text.first);
            }
        }
        classifyMarks(piece, loops, lines, marks, textPositions, smallest, tolerance, leftOut);
        applyLabels(piece, pieceTexts);
        applyPlainTexts(piece, pieceTexts);
    }

    // In reading order, as laid out in the drawing: rows from the top, left to right.
    const double row = qMax(largest * 0.25, 1e-9);
    std::stable_sort(pieces.begin(), pieces.end(), [row](const DxfPiece &a, const DxfPiece &b)
                     {
                         const int rowA = qFloor(-a.cutLine.boundingRect().bottom() / row);
                         const int rowB = qFloor(-b.cutLine.boundingRect().bottom() / row);
                         return rowA != rowB ? rowA < rowB
                                             : a.cutLine.boundingRect().left() < b.cutLine.boundingRect().left();
                     });

    int outside = 0;
    for (const QPolygonF &line : qAsConst(leftover))
    {
        bool inPiece = false;
        for (const DxfPiece &piece : qAsConst(pieces))
        {
            inPiece = inPiece || isInside(line, piece.cutLine);
        }
        outside += inPiece ? 0 : 1;
    }
    if (outside > 0)
    {
        warnings.append(DxfPatternReader::tr("%n line(s) outside the pieces were left out.", nullptr, outside));
    }
    if (leftOut > 0)
    {
        warnings.append(DxfPatternReader::tr("%n small outline(s) on the pieces (text drawn as lines, marks) were left "
                                             "out.", nullptr, leftOut));
    }
    return pieces;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief isAamaBlock is true for a block drawing an AAMA piece: an outline on layer 1, or piece texts. */
bool isAamaBlock(const DxfBlock &block)
{
    for (const DxfEntity &entity : block.entities)
    {
        if (aamaLayer(entity) == 1 && entity.isCurve())
        {
            return true;
        }
        QString key;
        QString value;
        if ((entity.type == QLatin1String("TEXT") || entity.type == QLatin1String("MTEXT"))
            && labelValue(entity.text, key, value) && key == QLatin1String("piece name"))
        {
            return true;
        }
    }
    return false;
}

} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
const QPolygonF &DxfPiece::outline() const
{
    return sewLine.isEmpty() ? cutLine : sewLine;
}

//---------------------------------------------------------------------------------------------------------------------
QRectF DxfPiece::bounds() const
{
    return cutLine.boundingRect().united(sewLine.boundingRect());
}

//---------------------------------------------------------------------------------------------------------------------
double DxfPattern::toCentimeters(const QString &unit)
{
    if (unit == QLatin1String("mm"))
    {
        return 0.1;
    }
    if (unit == QLatin1String("inch"))
    {
        return 2.54;
    }
    if (unit == QLatin1String("m"))
    {
        return 100.0;
    }
    if (unit == QLatin1String("ft"))
    {
        return 30.48;
    }
    return 1.0;
}

//---------------------------------------------------------------------------------------------------------------------
double DxfPatternReader::flatteningTolerance(const QString &unit)
{
    // A twentieth of a millimeter.
    return 0.005 / DxfPattern::toCentimeters(unit);
}

//---------------------------------------------------------------------------------------------------------------------
DxfPattern DxfPatternReader::extract(const DxfDocument &document)
{
    DxfPattern pattern;

    // Units: AAMA's UNITS text, then the drawing's header, else guessed below from the size of the pieces.
    QStringList texts;
    collectTexts(document.entities, texts);
    for (const DxfBlock &block : document.blocks)
    {
        collectTexts(block.entities, texts);
    }
    for (const QString &text : qAsConst(texts))
    {
        QString key;
        QString value;
        if (labelValue(text, key, value))
        {
            if (key == QLatin1String("units") && pattern.unit.isEmpty())
            {
                pattern.unit = unitFromName(value);
                if (!pattern.unit.isEmpty())
                {
                    pattern.unitSource = tr("UNITS: %1 in the file").arg(value.trimmed());
                }
            }
            else if (key == QLatin1String("style name") && pattern.styleName.isEmpty())
            {
                pattern.styleName = value;
            }
        }
        if (text.contains(QLatin1String("ASTM"), Qt::CaseInsensitive)
            || text.contains(QLatin1String("AAMA"), Qt::CaseInsensitive))
        {
            pattern.astm = true;
        }
    }
    if (pattern.unit.isEmpty())
    {
        static const QHash<QString, QString> insunits{{QStringLiteral("1"), QStringLiteral("inch")},
                                                      {QStringLiteral("2"), QStringLiteral("ft")},
                                                      {QStringLiteral("4"), QStringLiteral("mm")},
                                                      {QStringLiteral("5"), QStringLiteral("cm")},
                                                      {QStringLiteral("6"), QStringLiteral("m")}};
        const QString code = document.header.value(QStringLiteral("$INSUNITS"));
        if (insunits.contains(code))
        {
            pattern.unit = insunits.value(code);
            pattern.unitSource = tr("the drawing's units ($INSUNITS)");
        }
    }

    // Curves are flattened finely; without known units, relative to the size of the drawing.
    double tolerance = flatteningTolerance(pattern.unit.isEmpty() ? QStringLiteral("cm") : pattern.unit);
    if (pattern.unit.isEmpty())
    {
        QRectF extent;
        for (const DxfEntity &entity : document.entities)
        {
            for (const QPointF &point : entity.points)
            {
                extent = extent.united(QRectF(point, QSizeF(1e-9, 1e-9)));
            }
        }
        for (const DxfBlock &block : document.blocks)
        {
            for (const DxfEntity &entity : block.entities)
            {
                for (const QPointF &point : entity.points)
                {
                    extent = extent.united(QRectF(point - block.base, QSizeF(1e-9, 1e-9)));
                }
            }
        }
        tolerance = qMax(1e-6, qMax(extent.width(), extent.height()) * 2e-5);
    }

    // AAMA pieces: blocks with an outline on layer 1, placed where they are inserted (or as they are).
    QSet<QString> placed;
    for (const DxfEntity &entity : document.entities)
    {
        if (entity.type != QLatin1String("INSERT"))
        {
            continue;
        }
        const DxfBlock *block = document.block(entity.text);
        if (block == nullptr || !isAamaBlock(*block) || placed.contains(block->name.toUpper()))
        {
            continue;
        }
        const QTransform transform =
            insertTransform(block->base, entity.points.value(0), entity.scale, entity.rotation);
        DxfPiece piece = aamaPiece(*block, transform, tolerance);
        if (piece.cutLine.size() >= 3)
        {
            pattern.pieces.append(piece);
            placed.insert(block->name.toUpper());
        }
        else
        {
            pattern.warnings.append(tr("Piece %1 has no closed outline; it was left out.").arg(block->name));
        }
    }
    if (pattern.pieces.isEmpty())
    {
        for (const DxfBlock &block : document.blocks)
        {
            if (!isAamaBlock(block) || block.name.startsWith(QLatin1Char('*')))
            {
                continue;
            }
            const QTransform transform = QTransform::fromTranslate(-block.base.x(), -block.base.y());
            DxfPiece piece = aamaPiece(block, transform, tolerance);
            if (piece.cutLine.size() >= 3)
            {
                pattern.pieces.append(piece);
            }
        }
    }
    if (!pattern.pieces.isEmpty())
    {
        pattern.astm = true;
    }
    else
    {
        pattern.pieces = plainPieces(document, tolerance, pattern.warnings);
    }

    for (int i = 0; i < pattern.pieces.size(); ++i)
    {
        DxfPiece &piece = pattern.pieces[i];
        if (piece.name.trimmed().isEmpty())
        {
            piece.name = tr("Piece %1").arg(i + 1);
        }
        // Notches start on the edge they are cut into, the cut line.
        for (QLineF &notch : piece.notches)
        {
            if (distanceToOutline(notch.p2(), piece.cutLine) < distanceToOutline(notch.p1(), piece.cutLine))
            {
                notch = QLineF(notch.p2(), notch.p1());
            }
        }
    }

    if (pattern.unit.isEmpty())
    {
        // Guess from the size of the largest piece: a piece is 5 cm to 1.5 m long.
        double size = 0;
        for (const DxfPiece &piece : qAsConst(pattern.pieces))
        {
            size = qMax(size, qMax(piece.bounds().width(), piece.bounds().height()));
        }
        pattern.unit = size > 300 ? QStringLiteral("mm") : size > 75 ? QStringLiteral("cm") : QStringLiteral("inch");
        pattern.unitSource = tr("guessed from the size of the pieces");
    }

    if (pattern.pieces.isEmpty())
    {
        pattern.warnings.append(tr("No pattern pieces found: no closed outlines in the drawing."));
    }
    return pattern;
}

} // namespace Seamly3D
