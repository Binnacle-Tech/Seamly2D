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
        else if (edge.bulge < -0.5)
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

} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
QVector<Seam> SeamFinder::find(const QVector<GarmentPanel> &panels)
{
    QVector<EdgeInfo> edges;
    for (int p = 0; p < panels.size(); ++p)
    {
        const GarmentPanel &panel = panels.at(p);
        if (!panel.sewable || panel.edgeCount() < 2)
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
            info.hem = isHem(panel, e);
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
            // both are straight (side and shoulder seams). Two armholes, or two necklines, both curve in.
            if (qAbs(a.bulge + b.bulge) > qMax(1.0, 0.06 * a.length))
            {
                continue;
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
            const auto middleDistance = static_cast<double>((a.middle - b.middle).length());
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
    for (const Candidate &candidate : candidates)
    {
        const QPair<int, int> a(candidate.seam.panelA, candidate.seam.edgeA);
        const QPair<int, int> b(candidate.seam.panelB, candidate.seam.edgeB);
        if (used.contains(a) || used.contains(b))
        {
            continue;
        }
        used.insert(a);
        used.insert(b);
        seams.append(candidate.seam);
    }

    setInSleeves(panels, edges, used, seams);
    return seams;
}

} // namespace Seamly3D
