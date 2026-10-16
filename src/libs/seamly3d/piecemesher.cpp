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

//---------------------------------------------------------------------------------------------------------------------
double cross(const QVector3D &a, const QVector3D &b, const QVector3D &c)
{
    return static_cast<double>((b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x()));
}

//---------------------------------------------------------------------------------------------------------------------
double angleAt(const QVector3D &apex, const QVector3D &a, const QVector3D &b)
{
    const QVector3D u = (a - apex).normalized();
    const QVector3D v = (b - apex).normalized();
    return std::acos(qBound(-1.0, static_cast<double>(QVector3D::dotProduct(u, v)), 1.0));
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief delaunayFlips flips inner edges until the triangulation is Delaunay (Lawson): of the two ways to split each
 * pair of triangles, the one with the larger smallest angle. Outline edges, belonging to one triangle, never flip.
 */
void delaunayFlips(Mesh &mesh)
{
    const int triangleCount = static_cast<int>(mesh.indices.size() / 3);
    for (int round = 0; round < 64; ++round)
    {
        QHash<Edge, QPair<int, int>> triangles; // the one or two triangles of each edge
        for (int t = 0; t < triangleCount; ++t)
        {
            for (int k = 0; k < 3; ++k)
            {
                const Edge edge = edgeOf(mesh.indices.at(3 * t + k), mesh.indices.at(3 * t + (k + 1) % 3));
                auto it = triangles.find(edge);
                if (it == triangles.end())
                {
                    triangles.insert(edge, qMakePair(t, -1));
                }
                else
                {
                    it->second = t;
                }
            }
        }

        // Visit the edges triangle by triangle (not in hash order, which changes from run to run).
        QVector<bool> changed(triangleCount, false);
        bool flipped = false;
        for (int corner = 0; corner < 3 * triangleCount; ++corner)
        {
            const int t1 = corner / 3;
            const int k = corner % 3;
            if (changed.at(t1))
            {
                continue;
            }
            const auto pair =
                triangles.value(edgeOf(mesh.indices.at(3 * t1 + k), mesh.indices.at(3 * t1 + (k + 1) % 3)));
            const int t2 = pair.first == t1 ? pair.second : pair.first;
            if (t2 < 0 || changed.at(t2))
            {
                continue;
            }

            // t1 = (a, b, c) and t2 = (b, a, d), both wound the same way.
            const quint32 a = mesh.indices.at(3 * t1 + k);
            const quint32 b = mesh.indices.at(3 * t1 + (k + 1) % 3);
            const quint32 c = mesh.indices.at(3 * t1 + (k + 2) % 3);
            quint32 d = 0;
            for (int j = 0; j < 3; ++j)
            {
                const quint32 v = mesh.indices.at(3 * t2 + j);
                if (v != a && v != b)
                {
                    d = v;
                }
            }

            const QVector3D &pa = mesh.positions.at(static_cast<int>(a));
            const QVector3D &pb = mesh.positions.at(static_cast<int>(b));
            const QVector3D &pc = mesh.positions.at(static_cast<int>(c));
            const QVector3D &pd = mesh.positions.at(static_cast<int>(d));
            if (angleAt(pc, pa, pb) + angleAt(pd, pa, pb) <= M_PI + 1e-9)
            {
                continue; // already Delaunay
            }
            // The new triangles (a, d, c) and (d, b, c) must keep the winding: the pair forms a convex quad.
            const double winding = cross(pa, pb, pc);
            if (cross(pa, pd, pc) * winding <= 0 || cross(pd, pb, pc) * winding <= 0)
            {
                continue;
            }

            mesh.indices[3 * t1] = a;
            mesh.indices[3 * t1 + 1] = d;
            mesh.indices[3 * t1 + 2] = c;
            mesh.indices[3 * t2] = d;
            mesh.indices[3 * t2 + 1] = b;
            mesh.indices[3 * t2 + 2] = c;
            changed[t1] = true;
            changed[t2] = true;
            flipped = true;
        }
        if (!flipped)
        {
            break;
        }
    }
}

} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
QPolygonF PieceMesher::resample(const QPolygonF &outline, double maxEdge, QVector<int> *outlineIndex)
{
    // Drop repeated points (and a closing point equal to the first): zero length edges break the triangulation.
    QPolygonF clean;
    QVector<int> cleanIndex(outline.size(), 0);
    for (int i = 0; i < outline.size(); ++i)
    {
        if (clean.isEmpty() || QLineF(clean.constLast(), outline.at(i)).length() > 1e-6)
        {
            clean.append(outline.at(i));
        }
        cleanIndex[i] = static_cast<int>(clean.size()) - 1;
    }
    while (clean.size() > 1 && QLineF(clean.constFirst(), clean.constLast()).length() <= 1e-6)
    {
        const int last = static_cast<int>(clean.size()) - 1;
        clean.removeLast();
        for (int &index : cleanIndex)
        {
            if (index == last)
            {
                index = 0;
            }
        }
    }

    QPolygonF result;
    QVector<int> start(clean.size(), 0);
    for (int i = 0; i < clean.size(); ++i)
    {
        const QPointF &a = clean.at(i);
        const QPointF &b = clean.at((i + 1) % clean.size());
        const int parts = qMax(1, qCeil(QLineF(a, b).length() / maxEdge));
        start[i] = static_cast<int>(result.size());
        for (int k = 0; k < parts; ++k)
        {
            result.append(a + (b - a) * (static_cast<qreal>(k) / parts));
        }
    }

    if (outlineIndex != nullptr)
    {
        outlineIndex->resize(outline.size());
        for (int i = 0; i < outline.size(); ++i)
        {
            (*outlineIndex)[i] = clean.isEmpty() ? 0 : start.at(cleanIndex.at(i));
        }
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
Mesh PieceMesher::mesh(const QPolygonF &outline, double maxEdge, QVector<int> *outlineVertex)
{
    // The triangulation keeps the outline points as its first vertices, in order.
    const QPolygonF boundary = resample(outline, maxEdge, outlineVertex);
    Mesh mesh = PolygonTriangulator::flatPieceMesh(boundary);
    if (mesh.isEmpty())
    {
        return mesh;
    }
    // Ear clipping leaves long thin triangles; bisecting those would cascade into needlessly tiny ones.
    delaunayFlips(mesh);

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
            if (longestLength <= maxEdge * (1 + 1e-6) && !midpoints.contains(edge)) // outline edges are never split
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
        delaunayFlips(mesh);
    }

    return mesh;
}

} // namespace Seamly3D
