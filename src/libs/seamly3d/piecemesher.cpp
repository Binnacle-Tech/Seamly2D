/******************************************************************************
 *   @file   piecemesher.cpp
 **  @author Seamly2D project
 **  @date   15 10, 2026
 **
 **  @brief  Flat triangle meshes of pattern pieces.
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

#include "piecemesher.h"

#include <QHash>
#include <QLineF>
#include <QtMath>

#include <functional>

#include "polygontriangulator.h"

namespace Seamly3D
{

namespace
{

using Edge = QPair<quint32, quint32>;

Edge edgeOf(quint32 a, quint32 b)
{
    return a < b ? Edge(a, b) : Edge(b, a);
}

} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
QPolygonF PieceMesher::resample(const QPolygonF &outline, double maxEdge)
{
    QPolygonF points = outline;
    if (points.size() > 1 && points.constFirst() == points.constLast())
    {
        points.removeLast();
    }

    // Drop repeated points: they make zero length edges the triangulator can not use.
    QPolygonF clean;
    for (const QPointF &point : points)
    {
        if (clean.isEmpty() || QLineF(clean.constLast(), point).length() > 1e-6)
        {
            clean.append(point);
        }
    }
    if (clean.size() > 1 && QLineF(clean.constFirst(), clean.constLast()).length() <= 1e-6)
    {
        clean.removeLast();
    }

    QPolygonF result;
    for (int i = 0; i < clean.size(); ++i)
    {
        const QPointF &a = clean.at(i);
        const QPointF &b = clean.at((i + 1) % clean.size());
        const int parts = qMax(1, qCeil(QLineF(a, b).length() / maxEdge));
        for (int k = 0; k < parts; ++k)
        {
            result.append(a + (b - a) * (static_cast<qreal>(k) / parts));
        }
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
Mesh PieceMesher::mesh(const QPolygonF &outline, double maxEdge)
{
    const QPolygonF boundary = resample(outline, maxEdge);
    Mesh mesh = PolygonTriangulator::flatPieceMesh(boundary);
    if (mesh.isEmpty())
    {
        return mesh;
    }

    // Longest edge bisection (Rivara): split a triangle's longest edge, and the neighbour sharing it, until no edge
    // is too long. Splitting the longest edge keeps the triangles from getting thinner.
    const auto length = [&mesh](quint32 a, quint32 b)
    { return static_cast<double>((mesh.positions.at(static_cast<int>(a)) - mesh.positions.at(static_cast<int>(b))).length()); };

    for (int pass = 0; pass < 64; ++pass)
    {
        // Each long edge is split once per pass, at its midpoint, in every triangle that has it as its longest edge.
        QHash<Edge, quint32> midpoints;
        QVector<quint32> next;
        next.reserve(mesh.indices.size() * 2);
        bool split = false;

        for (int t = 0; t + 2 < mesh.indices.size(); t += 3)
        {
            const quint32 v[3] = {mesh.indices.at(t), mesh.indices.at(t + 1), mesh.indices.at(t + 2)};
            int longest = 0;
            double longestLength = 0;
            for (int k = 0; k < 3; ++k)
            {
                const double l = length(v[k], v[(k + 1) % 3]);
                if (l > longestLength)
                {
                    longestLength = l;
                    longest = k;
                }
            }

            const Edge edge = edgeOf(v[longest], v[(longest + 1) % 3]);
            if (longestLength <= maxEdge && !midpoints.contains(edge))
            {
                next << v[0] << v[1] << v[2];
                continue;
            }

            auto it = midpoints.find(edge);
            if (it == midpoints.end())
            {
                const auto index = static_cast<quint32>(mesh.positions.size());
                mesh.positions.append((mesh.positions.at(static_cast<int>(edge.first))
                                       + mesh.positions.at(static_cast<int>(edge.second)))
                                      / 2);
                mesh.normals.append(QVector3D(0, 0, 1));
                it = midpoints.insert(edge, index);
            }

            const quint32 a = v[longest];
            const quint32 b = v[(longest + 1) % 3];
            const quint32 c = v[(longest + 2) % 3];
            const quint32 m = it.value();
            next << a << m << c << m << b << c;
            split = true;
        }

        // Neighbours of a split edge now have a midpoint hanging on one, two or three of their edges: split them at
        // each of those midpoints so the mesh has no T-junctions.
        QVector<quint32> conforming;
        conforming.reserve(next.size() * 2);
        std::function<void(quint32, quint32, quint32)> addTriangle = [&](quint32 a, quint32 b, quint32 c)
        {
            const quint32 v[3] = {a, b, c};
            for (int k = 0; k < 3; ++k)
            {
                const auto it = midpoints.constFind(edgeOf(v[k], v[(k + 1) % 3]));
                if (it != midpoints.constEnd())
                {
                    const quint32 p = v[k];
                    const quint32 q = v[(k + 1) % 3];
                    const quint32 r = v[(k + 2) % 3];
                    addTriangle(p, it.value(), r);
                    addTriangle(it.value(), q, r);
                    return;
                }
            }
            conforming << a << b << c;
        };
        for (int t = 0; t + 2 < next.size(); t += 3)
        {
            addTriangle(next.at(t), next.at(t + 1), next.at(t + 2));
        }

        mesh.indices = conforming;
        if (!split)
        {
            break;
        }
    }

    return mesh;
}

} // namespace Seamly3D
