/******************************************************************************
 *   @file   garment.cpp
 **  @author Seamly2D project
 **  @date   16 10, 2026
 **
 **  @brief  Garment panels and the seams between them.
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

#include "garment.h"

#include <QSet>
#include <QtMath>

#include <algorithm>
#include <limits>

namespace Seamly3D
{

//---------------------------------------------------------------------------------------------------------------------
int GarmentPanel::edgeCount() const
{
    return static_cast<int>(corners.size());
}

//---------------------------------------------------------------------------------------------------------------------
QVector<int> GarmentPanel::edgeVertices(int edge) const
{
    QVector<int> vertices;
    if (corners.isEmpty() || outlineCount <= 0)
    {
        return vertices;
    }

    const int start = corners.at(edge % corners.size());
    int end = corners.at((edge + 1) % corners.size());
    if (end <= start)
    {
        end += outlineCount; // the last edge wraps around to the first corner
    }
    for (int v = start; v <= end; ++v)
    {
        vertices.append(v % outlineCount);
    }
    return vertices;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<double> GarmentPanel::edgeNotches(int edge) const
{
    QVector<double> result;
    const QVector<int> vertices = edgeVertices(edge);
    if (vertices.size() < 3 || notches.isEmpty())
    {
        return result;
    }
    double along = 0;
    QVector<double> at(vertices.size(), 0.0);
    for (int i = 1; i < vertices.size(); ++i)
    {
        along += static_cast<double>(
            (rest.positions.at(vertices.at(i)) - rest.positions.at(vertices.at(i - 1))).length());
        at[i] = along;
    }
    // Inside the edge only: a notch at a corner lines nothing up.
    for (int i = 1; i + 1 < vertices.size(); ++i)
    {
        if (notches.contains(vertices.at(i)) && along > 1e-9)
        {
            result.append(at.at(i) / along);
        }
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
double GarmentPanel::edgeLength(int edge) const
{
    const QVector<int> vertices = edgeVertices(edge);
    double length = 0;
    for (int i = 1; i < vertices.size(); ++i)
    {
        length += static_cast<double>(
            (rest.positions.at(vertices.at(i)) - rest.positions.at(vertices.at(i - 1))).length());
    }
    return length;
}

namespace
{

//---------------------------------------------------------------------------------------------------------------------
struct EdgeInfo
{
    int       panel;
    int       edge;
    double    length;
    QVector3D start;  // 3D, as arranged
    QVector3D end;
    QVector3D middle;
    bool      hem;
    double    bulge;
};

//---------------------------------------------------------------------------------------------------------------------
/** @brief A hem runs along the bottom of the flat piece (pattern Y points down). */
bool isHem(const GarmentPanel &panel, int edge)
{
    float bottom = std::numeric_limits<float>::lowest();
    for (int v = 0; v < panel.outlineCount; ++v)
    {
        bottom = qMax(bottom, panel.rest.positions.at(v).y());
    }

    const QVector<int> vertices = panel.edgeVertices(edge);
    for (const int v : vertices)
    {
        if (bottom - panel.rest.positions.at(v).y() > 1.5f) // centimeters above the bottom
        {
            return false;
        }
    }
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief A waistline runs straight along the top of the flat piece: the top of a skirt, of trousers, of a strapless
 * bodice. Shoulders slope, necklines curve down, so they are not.
 */
bool isWaistline(const GarmentPanel &panel, int edge)
{
    float top = std::numeric_limits<float>::max();
    for (int v = 0; v < panel.outlineCount; ++v)
    {
        top = qMin(top, panel.rest.positions.at(v).y());
    }

    const QVector<int> vertices = panel.edgeVertices(edge);
    for (const int v : vertices)
    {
        if (panel.rest.positions.at(v).y() - top > 1.5f) // centimeters below the top
        {
            return false;
        }
    }
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief How far the edge bulges out of the flat piece, from the straight line between its ends: positive when it
 * bulges out (a sleeve cap), negative when it curves into the piece (an armhole, a neckline), centimeters.
 */
double bulge(const GarmentPanel &panel, int edge)
{
    // Which side the inside of the piece is on depends on which way round the outline goes.
    double area = 0;
    for (int v = 0; v < panel.outlineCount; ++v)
    {
        const QVector3D &a = panel.rest.positions.at(v);
        const QVector3D &b = panel.rest.positions.at((v + 1) % panel.outlineCount);
        area += static_cast<double>(a.x() * b.y() - b.x() * a.y());
    }
    const double inside = area > 0 ? 1.0 : -1.0;

    const QVector<int> vertices = panel.edgeVertices(edge);
    const QVector3D start = panel.rest.positions.at(vertices.constFirst());
    const QVector3D chord = panel.rest.positions.at(vertices.constLast()) - start;
    const auto length = static_cast<double>(chord.length());
    if (length < 1e-6)
    {
        return 0;
    }
    double result = 0;
    for (const int v : vertices)
    {
        const QVector3D offset = panel.rest.positions.at(v) - start;
        const double side = -inside * static_cast<double>(chord.x() * offset.y() - chord.y() * offset.x()) / length;
        if (qAbs(side) > qAbs(result))
        {
            result = side;
        }
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief The angle at the corner two edges share, measured on the flat piece (radians). */
double cornerAngle(const GarmentPanel &panel, int corner)
{
    const QVector<int> before = panel.edgeVertices((corner + panel.edgeCount() - 1) % panel.edgeCount());
    const QVector<int> after = panel.edgeVertices(corner);
    if (before.size() < 2 || after.size() < 2)
    {
        return M_PI;
    }
    const QVector3D apex = panel.rest.positions.at(after.constFirst());
    const QVector3D a = (panel.rest.positions.at(before.at(before.size() - 2)) - apex).normalized();
    const QVector3D b = (panel.rest.positions.at(after.at(1)) - apex).normalized();
    return std::acos(qBound(-1.0, static_cast<double>(QVector3D::dotProduct(a, b)), 1.0));
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief setInSleeves sews each sleeve cap left over to the front and back armholes left over beside it.
 */
void setInSleeves(const QVector<GarmentPanel> &panels, const QVector<EdgeInfo> &edges, QSet<QPair<int, int>> &used,
                  QVector<Seam> &seams)
{
    QVector<EdgeInfo> caps;
    QVector<EdgeInfo> armholes;
    for (const EdgeInfo &edge : edges)
    {
        if (used.contains(qMakePair(edge.panel, edge.edge)))
        {
            continue;
        }
        if (edge.bulge > 1.0)
        {
            caps.append(edge);
        }
        else if (edge.bulge < -0.5 && !panels.at(edge.panel).sleeve)
        {
            armholes.append(edge);
        }
    }
    // Longest caps first.
    std::sort(caps.begin(), caps.end(), [](const EdgeInfo &a, const EdgeInfo &b) { return a.length > b.length; });

    // The underarm end of an armhole is its lower end, as arranged around the body.
    const auto underarm = [](const EdgeInfo &edge) { return edge.start.y() < edge.end.y() ? edge.start : edge.end; };

    for (const EdgeInfo &cap : caps)
    {
        double bestScore = std::numeric_limits<double>::max();
        int bestFirst = -1;
        int bestSecond = -1;
        for (int i = 0; i < armholes.size(); ++i)
        {
            for (int j = 0; j < armholes.size(); ++j)
            {
                const EdgeInfo &first = armholes.at(i);
                const EdgeInfo &second = armholes.at(j);
                if (i == j || first.panel == cap.panel || second.panel == cap.panel || first.panel == second.panel
                    || used.contains(qMakePair(first.panel, first.edge))
                    || used.contains(qMakePair(second.panel, second.edge)))
                {
                    continue;
                }
                // Same side of the body as the sleeve.
                if (first.middle.x() * cap.middle.x() <= 0 || second.middle.x() * cap.middle.x() <= 0)
                {
                    continue;
                }
                const double ease = cap.length / (first.length + second.length);
                if (ease < 0.97 || ease > 1.15)
                {
                    continue;
                }
                // The two armholes meet at the underarm (the side seam), and both are near the cap.
                const auto underarmGap = static_cast<double>((underarm(first) - underarm(second)).length());
                const auto reach = static_cast<double>((first.middle - cap.middle).length()
                                                       + (second.middle - cap.middle).length())
                                   / 2;
                if (underarmGap > 20.0 || reach > 30.0)
                {
                    continue;
                }

                // Which armhole the first part of the cap goes to: the one nearer to it.
                const GarmentPanel &capPanel = panels.at(cap.panel);
                const QVector<int> capVertices = capPanel.edgeVertices(cap.edge);
                const double share = first.length / (first.length + second.length);
                const QVector3D firstPart =
                    capPanel.shape.positions.at(capVertices.at(static_cast<int>(capVertices.size() * share / 2)));
                const auto side = static_cast<double>((firstPart - first.middle).length());

                const double score = qAbs(ease - 1.05) * 10 + underarmGap / 10 + reach / 10 + side / 10;
                if (score < bestScore)
                {
                    bestScore = score;
                    bestFirst = i;
                    bestSecond = j;
                }
            }
        }
        if (bestFirst < 0)
        {
            continue;
        }

        // The cap goes from the underarm along the first armhole to the shoulder, then along the second armhole back
        // to the underarm.
        const EdgeInfo &first = armholes.at(bestFirst);
        const EdgeInfo &second = armholes.at(bestSecond);
        const double share = first.length / (first.length + second.length);

        Seam toFirst;
        toFirst.panelA = cap.panel;
        toFirst.edgeA = cap.edge;
        toFirst.startA = 0;
        toFirst.endA = share;
        toFirst.panelB = first.panel;
        toFirst.edgeB = first.edge;
        toFirst.reversed = underarm(first) != first.start; // runs from its underarm end
        seams.append(toFirst);

        Seam toSecond = toFirst;
        toSecond.startA = share;
        toSecond.endA = 1;
        toSecond.panelB = second.panel;
        toSecond.edgeB = second.edge;
        toSecond.reversed = underarm(second) != second.end; // runs to its underarm end
        seams.append(toSecond);

        used.insert(qMakePair(cap.panel, cap.edge));
        used.insert(qMakePair(first.panel, first.edge));
        used.insert(qMakePair(second.panel, second.edge));
    }
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Sews each waistband and collar on: its lower edge along the edges left open around it (waistlines,
 * necklines), in order around the body, each taking a share of the band as long as itself.
 */
void sewOnBands(const QVector<GarmentPanel> &panels, QSet<QPair<int, int>> &used, QVector<Seam> &seams)
{
    for (int b = 0; b < panels.size(); ++b)
    {
        const GarmentPanel &band = panels.at(b);
        if (!band.sewable || !band.band || band.edgeCount() < 2)
        {
            continue;
        }

        // The lower of the band's long edges.
        double longest = 0;
        for (int e = 0; e < band.edgeCount(); ++e)
        {
            longest = qMax(longest, band.edgeLength(e));
        }
        int lower = -1;
        float lowest = std::numeric_limits<float>::max();
        for (int e = 0; e < band.edgeCount(); ++e)
        {
            if (band.edgeLength(e) < longest * 0.5 || used.contains(qMakePair(b, e)))
            {
                continue;
            }
            float height = 0;
            const QVector<int> vertices = band.edgeVertices(e);
            for (const int v : vertices)
            {
                height += band.shape.positions.at(v).y();
            }
            height /= static_cast<float>(vertices.size());
            if (height < lowest)
            {
                lowest = height;
                lower = e;
            }
        }
        if (lower < 0)
        {
            continue;
        }
        const QVector<int> line = band.edgeVertices(lower);
        QVector<double> along(line.size(), 0.0);
        for (int i = 1; i < line.size(); ++i)
        {
            along[i] = along.at(i - 1) + static_cast<double>((band.rest.positions.at(line.at(i))
                                                              - band.rest.positions.at(line.at(i - 1)))
                                                                 .length());
        }
        // Where a point is along the band's lower edge (0 to 1), and how far from it.
        const auto project = [&band, &line, &along](const QVector3D &point, float *distance)
        {
            int nearest = 0;
            float best = std::numeric_limits<float>::max();
            for (int i = 0; i < line.size(); ++i)
            {
                const float d = (band.shape.positions.at(line.at(i)) - point).length();
                if (d < best)
                {
                    best = d;
                    nearest = i;
                }
            }
            if (distance != nullptr)
            {
                *distance = best;
            }
            return along.at(nearest) / qMax(1e-9, along.constLast());
        };
        // Half a band (one side of the body) takes only the edges on its side.
        float side = 0;
        for (const int v : line)
        {
            side += band.shape.positions.at(v).x();
        }
        side /= static_cast<float>(line.size());

        struct Part
        {
            int    panel;
            int    edge;
            double from;   // along the band, where the edge's ends project
            double to;
            double length;
        };
        QVector<Part> parts;
        for (int p = 0; p < panels.size(); ++p)
        {
            const GarmentPanel &panel = panels.at(p);
            if (!panel.sewable || panel.band)
            {
                continue;
            }
            for (int e = 0; e < panel.edgeCount(); ++e)
            {
                if (used.contains(qMakePair(p, e)) || isHem(panel, e) || panel.edgeLength(e) < 2.0)
                {
                    continue;
                }
                // Near the band, and at its height: a neckline drops in front, but the tops of panels hanging below a
                // yoke are further down.
                const QVector<int> vertices = panel.edgeVertices(e);
                bool near = true;
                for (const int v : vertices)
                {
                    float distance = 0;
                    const QVector3D &point = panel.shape.positions.at(v);
                    project(point, &distance);
                    const float below = lowest - point.y();
                    near = near && distance <= SeamFinder::bandReach && below <= SeamFinder::bandDrop
                           && below >= -SeamFinder::bandRise;
                }
                const float middle = panel.shape.positions.at(vertices.at(vertices.size() / 2)).x();
                if (!near || (qAbs(side) > 3.0f && middle * side < 0))
                {
                    continue;
                }
                // A whole band starts and ends at the same place (the center front): an end of the edge there could be
                // either; it is the one on the side of the edge's middle.
                const double middleAt = project(panel.shape.positions.at(vertices.at(vertices.size() / 2)), nullptr);
                const auto endAt = [&project, &panel, middleAt](int vertex)
                {
                    const double at = project(panel.shape.positions.at(vertex), nullptr);
                    return qAbs(at - middleAt) > 0.5 ? (at > middleAt ? 0.0 : 1.0) : at;
                };
                parts.append({p, e, endAt(vertices.constFirst()), endAt(vertices.constLast()), panel.edgeLength(e)});
            }
        }
        if (parts.isEmpty())
        {
            continue;
        }

        // In order along the band, sharing it out from the first edge's start to the last one's end.
        std::sort(parts.begin(), parts.end(), [](const Part &x, const Part &y)
                  { return qMin(x.from, x.to) + qMax(x.from, x.to) < qMin(y.from, y.to) + qMax(y.from, y.to); });
        double start = 1;
        double end = 0;
        double total = 0;
        for (const Part &part : qAsConst(parts))
        {
            start = qMin(start, qMin(part.from, part.to));
            end = qMax(end, qMax(part.from, part.to));
            total += part.length;
        }
        if (end - start < 0.05)
        {
            start = 0;
            end = 1;
        }
        double at = start;
        for (const Part &part : qAsConst(parts))
        {
            Seam seam;
            seam.panelA = b;
            seam.edgeA = lower;
            seam.startA = at;
            at += (end - start) * part.length / total;
            seam.endA = at;
            seam.panelB = part.panel;
            seam.edgeB = part.edge;
            seam.reversed = part.from > part.to; // its end comes first along the band
            seams.append(seam);
            used.insert(qMakePair(part.panel, part.edge));
        }
        used.insert(qMakePair(b, lower));
    }
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief Where each point of an edge lies along it, 0 to 1, measured on the flat piece. */
QVector<double> edgeShares(const GarmentPanel &panel, const QVector<int> &vertices)
{
    QVector<double> along(vertices.size(), 0.0);
    for (int i = 1; i < vertices.size(); ++i)
    {
        along[i] = along.at(i - 1) + static_cast<double>((panel.rest.positions.at(vertices.at(i))
                                                          - panel.rest.positions.at(vertices.at(i - 1)))
                                                             .length());
    }
    const double total = qMax(1e-9, along.constLast());
    for (double &value : along)
    {
        value /= total;
    }
    return along;
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Sews an edge still open to the open edges of other pieces lying along it, when they are as long as it
 * together: a yoke's lower edge to the tops of the panels below it. Both are straight, and so were left open as a
 * hem and as waistlines. Each takes a share of the edge as long as itself, in order along it.
 */
void sewOneToMany(const QVector<GarmentPanel> &panels, QSet<QPair<int, int>> &used, QVector<Seam> &seams,
                  bool withSleeves)
{
    struct Open
    {
        int    panel;
        int    edge;
        double length;
    };
    QVector<Open> open;
    for (int p = 0; p < panels.size(); ++p)
    {
        const GarmentPanel &panel = panels.at(p);
        if (!panel.sewable || panel.band || (panel.sleeve && !withSleeves))
        {
            continue;
        }
        for (int e = 0; e < panel.edgeCount(); ++e)
        {
            if (!used.contains(qMakePair(p, e)) && panel.edgeLength(e) >= 2.0)
            {
                open.append({p, e, panel.edgeLength(e)});
            }
        }
    }
    // Longest first: the long edge gathers the short ones, not the other way round.
    std::sort(open.begin(), open.end(), [](const Open &a, const Open &b) { return a.length > b.length; });

    for (const Open &whole : qAsConst(open))
    {
        if (used.contains(qMakePair(whole.panel, whole.edge)))
        {
            continue;
        }
        const GarmentPanel &panel = panels.at(whole.panel);
        const QVector<int> line = panel.edgeVertices(whole.edge);
        const QVector<double> along = edgeShares(panel, line);
        const auto project = [&panel, &line, &along](const QVector3D &point, float *distance)
        {
            int nearest = 0;
            float best = std::numeric_limits<float>::max();
            for (int i = 0; i < line.size(); ++i)
            {
                const float d = (panel.shape.positions.at(line.at(i)) - point).length();
                if (d < best)
                {
                    best = d;
                    nearest = i;
                }
            }
            *distance = best;
            return along.at(nearest);
        };

        struct Part
        {
            int    panel;
            int    edge;
            double from;
            double to;
            double length;
        };
        QVector<Part> parts;
        double total = 0;
        for (const Open &other : qAsConst(open))
        {
            if (other.panel == whole.panel || used.contains(qMakePair(other.panel, other.edge)))
            {
                continue;
            }
            const GarmentPanel &otherPanel = panels.at(other.panel);
            const QVector<int> vertices = otherPanel.edgeVertices(other.edge);
            bool near = true;
            float distance = 0;
            for (const int v : vertices)
            {
                project(otherPanel.shape.positions.at(v), &distance);
                near = near && distance <= SeamFinder::partReach;
            }
            if (!near)
            {
                continue;
            }
            const double from = project(otherPanel.shape.positions.at(vertices.constFirst()), &distance);
            const double to = project(otherPanel.shape.positions.at(vertices.constLast()), &distance);
            parts.append({other.panel, other.edge, from, to, other.length});
            total += other.length;
        }
        if (parts.isEmpty() || qAbs(total - whole.length) > SeamFinder::partsTolerance * whole.length)
        {
            continue;
        }

        std::sort(parts.begin(), parts.end(), [](const Part &x, const Part &y)
                  { return qMin(x.from, x.to) + qMax(x.from, x.to) < qMin(y.from, y.to) + qMax(y.from, y.to); });
        double at = 0;
        for (const Part &part : qAsConst(parts))
        {
            Seam seam;
            seam.panelA = whole.panel;
            seam.edgeA = whole.edge;
            seam.startA = at;
            at += part.length / total;
            seam.endA = qMin(1.0, at);
            seam.panelB = part.panel;
            seam.edgeB = part.edge;
            seam.reversed = part.from > part.to;
            seams.append(seam);
            used.insert(qMakePair(part.panel, part.edge));
        }
        used.insert(qMakePair(whole.panel, whole.edge));
    }
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Sews a band left over (an under collar) to the band it lies on (the upper collar), edge by edge where both
 * are open: the same shape, cut twice.
 */
void sewBandPairs(const QVector<GarmentPanel> &panels, QSet<QPair<int, int>> &used, QVector<Seam> &seams)
{
    for (int q = 0; q < panels.size(); ++q)
    {
        const GarmentPanel &over = panels.at(q);
        if (!over.sewable || !over.band)
        {
            continue;
        }
        bool sewn = false;
        for (int e = 0; e < over.edgeCount() && !sewn; ++e)
        {
            sewn = used.contains(qMakePair(q, e));
        }
        if (sewn)
        {
            continue; // sewn on already (to the neckline, the waistline)
        }
        for (int p = 0; p < panels.size(); ++p)
        {
            const GarmentPanel &under = panels.at(p);
            // Another piece (not the mirrored copy, the other half of the same collar), on the same side.
            if (p == q || under.pieceId == over.pieceId || under.copy != over.copy || !under.sewable || !under.band)
            {
                continue;
            }
            int matched = 0;
            for (int e = 0; e < over.edgeCount(); ++e)
            {
                for (int f = 0; f < under.edgeCount(); ++f)
                {
                    if (used.contains(qMakePair(p, f)) || used.contains(qMakePair(q, e)))
                    {
                        continue;
                    }
                    const double lengthA = over.edgeLength(e);
                    const double lengthB = under.edgeLength(f);
                    if (qAbs(lengthA - lengthB) > 0.05 * qMax(lengthA, lengthB))
                    {
                        continue;
                    }
                    const QVector<int> a = over.edgeVertices(e);
                    const QVector<int> b = under.edgeVertices(f);
                    const QVector3D a0 = over.shape.positions.at(a.constFirst());
                    const QVector3D a1 = over.shape.positions.at(a.constLast());
                    const QVector3D b0 = under.shape.positions.at(b.constFirst());
                    const QVector3D b1 = under.shape.positions.at(b.constLast());
                    const float same = (a0 - b0).length() + (a1 - b1).length();
                    const float crossed = (a0 - b1).length() + (a1 - b0).length();
                    if (qMin(same, crossed) > 2 * SeamFinder::partReach)
                    {
                        continue;
                    }
                    Seam seam;
                    seam.panelA = q;
                    seam.edgeA = e;
                    seam.panelB = p;
                    seam.edgeB = f;
                    seam.reversed = crossed < same;
                    seams.append(seam);
                    used.insert(qMakePair(q, e));
                    used.insert(qMakePair(p, f));
                    ++matched;
                }
            }
            if (matched > 0)
            {
                break;
            }
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief An edge of an opening left in the garment, walked one way round it. */
struct Stretch
{
    int    panel;
    int    edge;
    bool   forward; // from its start corner to its end corner
    double length;  // along the flat piece
    double offset;  // where it starts, along the opening
};

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Where a point at a corner of an edge sewn (share 0 or 1 of it) meets the piece it is sewn to: the corner
 * there, as the panel and the corner's index. False when no seam ends there, or it meets the middle of an edge.
 */
bool acrossSeam(const QVector<GarmentPanel> &panels, const QVector<Seam> &seams, int panel, int edge, double share,
                int *otherPanel, int *otherCorner, int *otherEdge)
{
    constexpr double near = 0.02;
    for (const Seam &seam : seams)
    {
        for (int side = 0; side < 2; ++side)
        {
            const bool isA = side == 0;
            if ((isA ? seam.panelA : seam.panelB) != panel || (isA ? seam.edgeA : seam.edgeB) != edge)
            {
                continue;
            }
            const double start = isA ? seam.startA : seam.startB;
            const double end = isA ? seam.endA : seam.endB;
            const bool atStart = qAbs(start - share) < near;
            if (!atStart && qAbs(end - share) >= near)
            {
                continue;
            }
            // The start of A's part meets the start of B's part, or its end when reversed.
            const double otherStart = isA ? seam.startB : seam.startA;
            const double otherEnd = isA ? seam.endB : seam.endA;
            const double there = (atStart != seam.reversed) ? otherStart : otherEnd;
            const int to = isA ? seam.panelB : seam.panelA;
            const int toEdge = isA ? seam.edgeB : seam.edgeA;
            if (there > near && there < 1 - near)
            {
                return false; // the middle of an edge: a T where three pieces meet
            }
            *otherPanel = to;
            *otherEdge = toEdge;
            *otherCorner = there < near ? toEdge : (toEdge + 1) % panels.at(to).edgeCount();
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Walks an opening left in the garment from an open edge: at each corner on along the next open edge, on the
 * same piece or, across the seams there, on the piece sewn to it. Empty when the walk does not come back round.
 */
QVector<Stretch> opening(const QVector<GarmentPanel> &panels, const QVector<Seam> &seams,
                         const QSet<QPair<int, int>> &open, int panel, int edge)
{
    QVector<Stretch> result;
    Stretch stretch{panel, edge, true, panels.at(panel).edgeLength(edge), 0.0};
    double offset = 0;
    for (int steps = 0; steps < 16; ++steps)
    {
        stretch.offset = offset;
        result.append(stretch);
        offset += stretch.length;

        // The corner it ends at, and the edge on from there.
        int at = stretch.panel;
        int from = stretch.edge;
        int corner = stretch.forward ? (from + 1) % panels.at(at).edgeCount() : from;
        bool found = false;
        for (int crossings = 0; crossings < 8; ++crossings)
        {
            const int count = panels.at(at).edgeCount();
            const int next = from == corner ? (corner + count - 1) % count : corner;
            if (open.contains(qMakePair(at, next)))
            {
                stretch = {at, next, next == corner, panels.at(at).edgeLength(next), 0.0};
                found = true;
                break;
            }
            int otherPanel = 0;
            int otherCorner = 0;
            int otherEdge = 0;
            if (!acrossSeam(panels, seams, at, next, next == corner ? 0.0 : 1.0, &otherPanel, &otherCorner,
                            &otherEdge))
            {
                return {};
            }
            at = otherPanel;
            from = otherEdge;
            corner = otherCorner;
        }
        if (!found)
        {
            return {};
        }
        if (stretch.panel == panel && stretch.edge == edge)
        {
            return stretch.forward ? result : QVector<Stretch>();
        }
        for (const Stretch &before : qAsConst(result))
        {
            if (before.panel == stretch.panel && before.edge == stretch.edge)
            {
                return {}; // round a loop that does not include the start
            }
        }
    }
    return {};
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief The point of an opening at a distance along it, as arranged. */
QVector3D pointAlong(const QVector<GarmentPanel> &panels, const QVector<Stretch> &stretches, double at)
{
    const double total = stretches.constLast().offset + stretches.constLast().length;
    at = std::fmod(std::fmod(at, total) + total, total);
    for (const Stretch &stretch : stretches)
    {
        if (at > stretch.offset + stretch.length && &stretch != &stretches.constLast())
        {
            continue;
        }
        const GarmentPanel &panel = panels.at(stretch.panel);
        QVector<int> vertices = panel.edgeVertices(stretch.edge);
        if (!stretch.forward)
        {
            std::reverse(vertices.begin(), vertices.end());
        }
        double along = stretch.offset;
        for (int i = 1; i < vertices.size(); ++i)
        {
            const QVector3D &a = panel.rest.positions.at(vertices.at(i - 1));
            const QVector3D &b = panel.rest.positions.at(vertices.at(i));
            const auto step = static_cast<double>((b - a).length());
            if (along + step >= at || i + 1 == vertices.size())
            {
                const auto t = static_cast<float>(step > 1e-9 ? qBound(0.0, (at - along) / step, 1.0) : 0.0);
                return panel.shape.positions.at(vertices.at(i - 1)) * (1 - t)
                       + panel.shape.positions.at(vertices.at(i)) * t;
            }
            along += step;
        }
    }
    return {};
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Sets in sleeves made of pieces sewn together (a top sleeve and an under sleeve): the opening their caps
 * leave, edge after edge round the top of the sleeve, is sewn into an armhole opening on the same side of the body,
 * however many pieces either is made of (a yoke and the panels below it). The top of the cap meets the top of the
 * armhole (the shoulder) and the rest is shared out evenly from there. Only sleeves of at least @p pieces pieces.
 */
void setInSleeveOpenings(const QVector<GarmentPanel> &panels, QSet<QPair<int, int>> &used, QVector<Seam> &seams,
                         int pieces)
{
    QSet<QPair<int, int>> open;
    for (int p = 0; p < panels.size(); ++p)
    {
        const GarmentPanel &panel = panels.at(p);
        if (!panel.sewable || panel.band || panel.edgeCount() < 2)
        {
            continue;
        }
        for (int e = 0; e < panel.edgeCount(); ++e)
        {
            if (!used.contains(qMakePair(p, e)) && !isHem(panel, e) && !isWaistline(panel, e))
            {
                open.insert(qMakePair(p, e));
            }
        }
    }

    struct Opening
    {
        QVector<Stretch> stretches;
        double           length;
        QVector3D        middle; // the average of its points, as arranged
        bool             sleeve;
        int              pieces;
    };
    QVector<Opening> openings;
    QSet<QPair<int, int>> walked;
    for (const QPair<int, int> &start : qAsConst(open))
    {
        if (walked.contains(start))
        {
            continue;
        }
        const QVector<Stretch> stretches = opening(panels, seams, open, start.first, start.second);
        if (stretches.isEmpty())
        {
            continue;
        }
        Opening found{stretches, stretches.constLast().offset + stretches.constLast().length, {}, true, 0};
        QSet<int> panelsIn;
        bool body = true;
        int count = 0;
        for (const Stretch &stretch : stretches)
        {
            walked.insert(qMakePair(stretch.panel, stretch.edge));
            found.sleeve = found.sleeve && panels.at(stretch.panel).sleeve;
            body = body && !panels.at(stretch.panel).sleeve;
            panelsIn.insert(stretch.panel);
            for (const int v : panels.at(stretch.panel).edgeVertices(stretch.edge))
            {
                found.middle += panels.at(stretch.panel).shape.positions.at(v);
                ++count;
            }
        }
        if (!found.sleeve && !body)
        {
            continue; // part sleeve, part body: not an opening either way
        }
        found.middle /= static_cast<float>(qMax(1, count));
        found.pieces = static_cast<int>(panelsIn.size());
        openings.append(found);
    }
    // Longest sleeves first.
    std::sort(openings.begin(), openings.end(), [](const Opening &a, const Opening &b) { return a.length > b.length; });

    QSet<int> taken;
    for (int c = 0; c < openings.size(); ++c)
    {
        const Opening &cap = openings.at(c);
        if (!cap.sleeve || cap.pieces < pieces || qAbs(cap.middle.x()) < 2.0f)
        {
            continue;
        }
        double bestScore = std::numeric_limits<double>::max();
        int best = -1;
        for (int a = 0; a < openings.size(); ++a)
        {
            const Opening &armhole = openings.at(a);
            // On the same side of the body (not a neckline across the middle).
            if (armhole.sleeve || taken.contains(a) || armhole.middle.x() * cap.middle.x() <= 0
                || qAbs(armhole.middle.x()) < 5.0f)
            {
                continue;
            }
            const double ease = cap.length / armhole.length;
            const auto reach = static_cast<double>((armhole.middle - cap.middle).length());
            if (ease < 0.97 || ease > 1.15 || reach > 30.0)
            {
                continue;
            }
            const double score = qAbs(ease - 1.05) * 10 + reach / 10;
            if (score < bestScore)
            {
                bestScore = score;
                best = a;
            }
        }
        if (best < 0)
        {
            continue;
        }
        taken.insert(best);
        QVector<Stretch> sleeve = cap.stretches;
        const QVector<Stretch> &armhole = openings.at(best).stretches;
        const double sleeveLength = cap.length;
        const double armholeLength = openings.at(best).length;

        // The top of the cap: the highest point, in the pattern, of the longest part of it (the top sleeve's).
        const Stretch *top = &sleeve.constFirst();
        for (const Stretch &stretch : qAsConst(sleeve))
        {
            top = stretch.length > top->length ? &stretch : top;
        }
        double capTop = top->offset;
        {
            const GarmentPanel &panel = panels.at(top->panel);
            QVector<int> vertices = panel.edgeVertices(top->edge);
            if (!top->forward)
            {
                std::reverse(vertices.begin(), vertices.end());
            }
            float highest = std::numeric_limits<float>::max();
            double along = top->offset;
            for (int i = 0; i < vertices.size(); ++i)
            {
                if (i > 0)
                {
                    along += static_cast<double>((panel.rest.positions.at(vertices.at(i))
                                                  - panel.rest.positions.at(vertices.at(i - 1)))
                                                     .length());
                }
                if (panel.rest.positions.at(vertices.at(i)).y() < highest)
                {
                    highest = panel.rest.positions.at(vertices.at(i)).y();
                    capTop = along;
                }
            }
        }
        // The top of the armhole: its highest point, as arranged (the shoulder).
        double shoulder = 0;
        {
            float highest = std::numeric_limits<float>::lowest();
            for (double at = 0; at < armholeLength; at += 0.5)
            {
                const float y = pointAlong(panels, armhole, at).y();
                if (y > highest)
                {
                    highest = y;
                    shoulder = at;
                }
            }
        }

        // Both walked the same way round: from the top toward the front, or both toward the back.
        const auto towardFront = [&panels](const QVector<Stretch> &stretches, double from, double length)
        {
            return pointAlong(panels, stretches, from + length / 4).z()
                   > pointAlong(panels, stretches, from - length / 4).z();
        };
        if (towardFront(sleeve, capTop, sleeveLength) != towardFront(armhole, shoulder, armholeLength))
        {
            std::reverse(sleeve.begin(), sleeve.end());
            double offset = 0;
            for (Stretch &stretch : sleeve)
            {
                stretch.forward = !stretch.forward;
                stretch.offset = offset;
                offset += stretch.length;
            }
            capTop = sleeveLength - capTop;
        }

        // Shared out evenly from the top: every corner of either opening starts a new seam.
        const auto share = [](double at, double from, double length)
        { return std::fmod(std::fmod(at - from, length) + length, length) / length; };
        QVector<double> breaks{0.0, 1.0};
        for (const Stretch &stretch : qAsConst(sleeve))
        {
            breaks.append(share(stretch.offset, capTop, sleeveLength));
        }
        for (const Stretch &stretch : armhole)
        {
            breaks.append(share(stretch.offset, shoulder, armholeLength));
        }
        std::sort(breaks.begin(), breaks.end());
        QVector<double> merged;
        for (const double at : qAsConst(breaks))
        {
            if (merged.isEmpty() || at - merged.constLast() > 0.01)
            {
                merged.append(at);
            }
        }
        merged.last() = 1.0;

        // Where a share of the way round lies on the stretch the middle of the part is on: a share of that edge.
        const auto onEdge = [](const QVector<Stretch> &stretches, double from, double length, double middle,
                               double at0, double at1, double *edge0, double *edge1)
        {
            const double m = std::fmod(from + middle * length, length);
            const Stretch *on = &stretches.constLast();
            for (const Stretch &stretch : stretches)
            {
                if (m < stretch.offset + stretch.length)
                {
                    on = &stretch;
                    break;
                }
            }
            // Measured from the middle of the part, which lies on this stretch (the part may begin at its end, a whole
            // way round from its start).
            const double fromMiddle = m - on->offset;
            const auto local = [&](double at)
            {
                double t = qBound(0.0, (fromMiddle + (at - middle) * length) / qMax(1e-9, on->length), 1.0);
                // At a corner of either opening merged with a point near it on the other: at the corner.
                t = t < 0.02 ? 0.0 : (t > 0.98 ? 1.0 : t);
                return on->forward ? t : 1 - t;
            };
            *edge0 = local(at0);
            *edge1 = local(at1);
            return on;
        };
        for (int i = 1; i < merged.size(); ++i)
        {
            const double middle = (merged.at(i - 1) + merged.at(i)) / 2;
            double a0 = 0;
            double a1 = 0;
            double b0 = 0;
            double b1 = 0;
            const Stretch *partA = onEdge(sleeve, capTop, sleeveLength, middle, merged.at(i - 1), merged.at(i), &a0,
                                          &a1);
            const Stretch *partB = onEdge(armhole, shoulder, armholeLength, middle, merged.at(i - 1), merged.at(i),
                                          &b0, &b1);
            if (qAbs(a1 - a0) * partA->length < 0.3 || qAbs(b1 - b0) * partB->length < 0.3)
            {
                continue;
            }
            Seam seam;
            seam.panelA = partA->panel;
            seam.edgeA = partA->edge;
            seam.startA = qMin(a0, a1);
            seam.endA = qMax(a0, a1);
            seam.panelB = partB->panel;
            seam.edgeB = partB->edge;
            seam.startB = qMin(b0, b1);
            seam.endB = qMax(b0, b1);
            seam.reversed = (a0 < a1) != (b0 < b1);
            seams.append(seam);
        }
        for (const Stretch &stretch : qAsConst(sleeve))
        {
            used.insert(qMakePair(stretch.panel, stretch.edge));
        }
        for (const Stretch &stretch : armhole)
        {
            used.insert(qMakePair(stretch.panel, stretch.edge));
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Of the seams found edge to edge, those that are two sides of an opening rather than a seam: two edges that
 * fan out at a sharp corner from the end of another seam between their pieces (the front and back armholes from the
 * end of the shoulder seam). Two such seams fan out from each other; the one whose edges curve in more is the
 * opening (the shorter one, if neither does). Returns the indices in @p chosen of those to leave open.
 */
QVector<int> fanningOut(const QVector<GarmentPanel> &panels, const QVector<Seam> &seams, const QVector<int> &chosen)
{
    // Where each edge's start (0) or end (1) meets the other edge: the seam and the share there.
    const auto meets = [&seams](int panel, int edge, int end, int *seamIndex, int *otherPanel, int *otherEdge,
                                int *otherEnd)
    {
        for (int i = 0; i < seams.size(); ++i)
        {
            const Seam &seam = seams.at(i);
            if (seam.panelA == seam.panelB)
            {
                continue;
            }
            const bool isA = seam.panelA == panel && seam.edgeA == edge;
            const bool isB = seam.panelB == panel && seam.edgeB == edge;
            if (!isA && !isB)
            {
                continue;
            }
            *seamIndex = i;
            *otherPanel = isA ? seam.panelB : seam.panelA;
            *otherEdge = isA ? seam.edgeB : seam.edgeA;
            *otherEnd = seam.reversed ? 1 - end : end;
            return true;
        }
        return false;
    };
    const auto curvesIn = [&panels](const Seam &seam)
    { return bulge(panels.at(seam.panelA), seam.edgeA) + bulge(panels.at(seam.panelB), seam.edgeB); };

    QVector<int> result;
    for (int i = 0; i < seams.size(); ++i)
    {
        const Seam &seam = seams.at(i);
        const GarmentPanel &a = panels.at(seam.panelA);
        const GarmentPanel &b = panels.at(seam.panelB);
        if (seam.panelA == seam.panelB || a.edgeCount() < 3 || b.edgeCount() < 3)
        {
            continue;
        }
        for (int end = 0; end < 2; ++end)
        {
            // The corners that meet at this end, and the edges on from them.
            const int endB = seam.reversed ? 1 - end : end;
            const int cornerA = (seam.edgeA + end) % a.edgeCount();
            const int cornerB = (seam.edgeB + endB) % b.edgeCount();
            const int nextA = end == 0 ? (seam.edgeA + a.edgeCount() - 1) % a.edgeCount() : cornerA;
            const int nextB = endB == 0 ? (seam.edgeB + b.edgeCount() - 1) % b.edgeCount() : cornerB;
            int other = 0;
            int otherPanel = 0;
            int otherEdge = 0;
            int otherEnd = 0;
            // nextA's end at the corner: its end when it comes before, its start when it comes after.
            if (!meets(seam.panelA, nextA, end == 0 ? 1 : 0, &other, &otherPanel, &otherEdge, &otherEnd)
                || other == i || otherPanel != seam.panelB || otherEdge != nextB || otherEnd != (endB == 0 ? 1 : 0))
            {
                continue;
            }
            const double sharp = qDegreesToRadians(150.0);
            if (cornerAngle(a, cornerA) > sharp || cornerAngle(b, cornerB) > sharp)
            {
                continue;
            }
            const double inThis = curvesIn(seam);
            const double inOther = curvesIn(seams.at(other));
            const bool thisOne = qAbs(inThis - inOther) > 0.2
                                     ? inThis < inOther
                                     : a.edgeLength(seam.edgeA) < panels.at(otherPanel).edgeLength(otherEdge);
            const int victim = chosen.at(thisOne ? i : other);
            if (!result.contains(victim))
            {
                result.append(victim);
            }
        }
    }
    return result;
}

} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
QVector<Seam> SeamFinder::find(const QVector<GarmentPanel> &panels)
{
    QVector<EdgeInfo> edges;
    for (int p = 0; p < panels.size(); ++p)
    {
        const GarmentPanel &panel = panels.at(p);
        if (!panel.sewable || panel.band || panel.edgeCount() < 2)
        {
            continue;
        }
        for (int e = 0; e < panel.edgeCount(); ++e)
        {
            const QVector<int> vertices = panel.edgeVertices(e);
            EdgeInfo info;
            info.panel = p;
            info.edge = e;
            info.length = panel.edgeLength(e);
            info.start = panel.shape.positions.at(vertices.constFirst());
            info.end = panel.shape.positions.at(vertices.constLast());
            info.middle = panel.shape.positions.at(vertices.at(vertices.size() / 2));
            info.hem = isHem(panel, e) || isWaistline(panel, e);
            info.bulge = bulge(panel, e);
            if (info.length >= 2.0 && !info.hem)
            {
                edges.append(info);
            }
        }
    }

    struct Candidate
    {
        double score;
        Seam   seam;
    };
    QVector<Candidate> candidates;

    for (int i = 0; i < edges.size(); ++i)
    {
        for (int j = i + 1; j < edges.size(); ++j)
        {
            const EdgeInfo &a = edges.at(i);
            const EdgeInfo &b = edges.at(j);
            const double lengthDifference = qAbs(a.length - b.length) / qMax(a.length, b.length);
            if (lengthDifference > lengthTolerance)
            {
                continue;
            }

            const GarmentPanel &panel = panels.at(a.panel);
            const bool samePanel = a.panel == b.panel;
            const bool neighbours = samePanel && ((a.edge + 1) % panel.edgeCount() == b.edge
                                                  || (b.edge + 1) % panel.edgeCount() == a.edge);

            Seam seam;
            seam.panelA = a.panel;
            seam.edgeA = a.edge;
            seam.panelB = b.panel;
            seam.edgeB = b.edge;

            if (neighbours)
            {
                // A dart: two legs of the same length meeting at a sharp point, sewn from the point outward.
                const int apex = (a.edge + 1) % panel.edgeCount() == b.edge ? b.edge : a.edge;
                if (cornerAngle(panel, apex) > qDegreesToRadians(40.0))
                {
                    continue;
                }
                seam.reversed = true;
                candidates.append({lengthDifference, seam});
                continue;
            }

            // Edges sewn together have the same shape: where one bulges out the other curves in (a princess seam), or
            // both are straight, or both bulge out alike (side seams over the hips). Two armholes, or two necklines,
            // both curve in; so do the inseams and crotch seams of trousers, but those already lie together once
            // arranged.
            const auto middleDistance = static_cast<double>((a.middle - b.middle).length());
            const double tolerance = qMax(1.0, 0.06 * a.length);
            if (qAbs(a.bulge + b.bulge) > tolerance)
            {
                const bool alike = qAbs(a.bulge - b.bulge) <= tolerance;
                const bool bothOut = alike && a.bulge > 0 && b.bulge > 0;
                const bool bothIn = alike && a.bulge < 0 && b.bulge < 0;
                // Both ends together too: the front and back armholes fan out from the top of the side seam.
                const auto endsApart = static_cast<double>(
                    qMin(qMax((a.start - b.start).length(), (a.end - b.end).length()),
                         qMax((a.start - b.end).length(), (a.end - b.start).length())));
                if (!bothOut && !(bothIn && middleDistance <= closeMiddles && endsApart <= closeMiddles))
                {
                    continue;
                }
            }

            // Same side of the body (seams do not cross the center), unless both sit on the center line.
            const bool centerA = qAbs(a.middle.x()) < 2.0f;
            const bool centerB = qAbs(b.middle.x()) < 2.0f;
            if (!(centerA && centerB) && a.middle.x() * b.middle.x() < 0)
            {
                continue;
            }

            // Edges sewn together run side by side: their ends and their middles are close. (The front and back
            // armholes share their ends, but their middles are apart.)
            const double same =
                (static_cast<double>((a.start - b.start).length() + (a.end - b.end).length()) + middleDistance) / 3;
            const double crossed =
                (static_cast<double>((a.start - b.end).length() + (a.end - b.start).length()) + middleDistance) / 3;
            const double distance = qMin(same, crossed);
            if (distance > maxDistance)
            {
                continue;
            }
            seam.reversed = crossed < same;

            // Two edges of one piece sewn together (the underarm seam of a sleeve) face each other across it. Edges
            // that lie end to end (the waist on both sides of a dart) do not.
            if (samePanel)
            {
                const QVector<int> va = panel.edgeVertices(a.edge);
                const QVector<int> vb = panel.edgeVertices(b.edge);
                const QVector3D a0 = panel.rest.positions.at(va.constFirst());
                const QVector3D a1 = panel.rest.positions.at(va.constLast());
                QVector3D b0 = panel.rest.positions.at(vb.constFirst());
                QVector3D b1 = panel.rest.positions.at(vb.constLast());
                if (seam.reversed)
                {
                    std::swap(b0, b1);
                }
                const QVector3D gap = ((b0 - a0) + (b1 - a1)) / 2;
                const QVector3D along = (a1 - a0).normalized();
                if (qAbs(static_cast<double>(QVector3D::dotProduct(gap, along))) > 0.5 * a.length)
                {
                    continue;
                }
            }
            candidates.append({lengthDifference * 100.0 + distance / 10.0, seam});
        }
    }

    // Best matches first; every edge is sewn once.
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate &a, const Candidate &b) { return a.score < b.score; });
    QSet<QPair<int, int>> used;
    QVector<Seam> seams;
    QSet<int> left; // candidates left open
    for (int round = 0; round < 4; ++round)
    {
        used.clear();
        seams.clear();
        QVector<int> chosen;
        for (int c = 0; c < candidates.size(); ++c)
        {
            const Seam &seam = candidates.at(c).seam;
            const QPair<int, int> a(seam.panelA, seam.edgeA);
            const QPair<int, int> b(seam.panelB, seam.edgeB);
            if (left.contains(c) || used.contains(a) || used.contains(b))
            {
                continue;
            }
            used.insert(a);
            used.insert(b);
            seams.append(seam);
            chosen.append(c);
        }
        const int before = static_cast<int>(left.size());
        for (const int c : fanningOut(panels, seams, chosen))
        {
            left.insert(c);
        }
        if (left.size() == before)
        {
            break;
        }
    }

    // The body's yokes first, so that an armhole across a yoke and the panels below it is one opening.
    sewOneToMany(panels, used, seams, false);
    setInSleeveOpenings(panels, used, seams, 2);
    setInSleeves(panels, edges, used, seams);
    setInSleeveOpenings(panels, used, seams, 1);
    sewOnBands(panels, used, seams);
    sewBandPairs(panels, used, seams);
    sewOneToMany(panels, used, seams, true);
    return seams;
}

} // namespace Seamly3D
