/******************************************************************************
 *   @file   meshslicer.cpp
 **  @author Seamly2D project
 **  @date   15 10, 2026
 **
 **  @brief  Plane cuts of meshes, used to measure girths on a body.
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

#include "meshslicer.h"

#include <QHash>
#include <QSet>

namespace
{

// A cut point is identified either by the mesh vertex it falls on or by the edge it crosses, so the segments of
// neighbouring triangles share endpoints exactly and can be chained without any distance tolerance.
using PointKey = quint64;

PointKey vertexKey(quint32 vertex)
{
    return (static_cast<PointKey>(vertex) << 32) | 0xFFFFFFFFu;
}

PointKey edgeKey(quint32 a, quint32 b)
{
    return a < b ? (static_cast<PointKey>(a) << 32) | b : (static_cast<PointKey>(b) << 32) | a;
}

} // anonymous namespace

namespace Seamly3D
{

//---------------------------------------------------------------------------------------------------------------------
double MeshSlicer::Contour::length() const
{
    double total = 0;
    for (int i = 1; i < points.size(); ++i)
    {
        total += static_cast<double>((points.at(i) - points.at(i - 1)).length());
    }
    if (closed && points.size() > 2)
    {
        total += static_cast<double>((points.constFirst() - points.constLast()).length());
    }
    return total;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<MeshSlicer::Contour> MeshSlicer::sliceAtHeight(const Mesh &mesh, float height)
{
    return sliceByPlane(mesh, QVector3D(0, height, 0), QVector3D(0, 1, 0));
}

//---------------------------------------------------------------------------------------------------------------------
QVector<MeshSlicer::Contour> MeshSlicer::sliceByPlane(const Mesh &mesh, const QVector3D &origin,
                                                      const QVector3D &normal)
{
    QHash<PointKey, QVector3D>        position;
    QHash<PointKey, QVector<PointKey>> neighbours;

    const auto distance = [&](quint32 v)
    { return QVector3D::dotProduct(mesh.positions.at(static_cast<int>(v)) - origin, normal); };

    // Vertices exactly on the plane count as above it; a cut through such a vertex lands on the vertex itself.
    const auto above = [&](quint32 v) { return distance(v) >= 0; };

    const auto cutPoint = [&](quint32 a, quint32 b) -> PointKey
    {
        const QVector3D &pa = mesh.positions.at(static_cast<int>(a));
        const QVector3D &pb = mesh.positions.at(static_cast<int>(b));
        const float da = distance(a);
        const float db = distance(b);
        if (da == 0)
        {
            position.insert(vertexKey(a), pa);
            return vertexKey(a);
        }
        if (db == 0)
        {
            position.insert(vertexKey(b), pb);
            return vertexKey(b);
        }
        const float t = da / (da - db);
        const PointKey key = edgeKey(a, b);
        position.insert(key, pa + (pb - pa) * t);
        return key;
    };

    for (int i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        const quint32 v[3] = {mesh.indices.at(i), mesh.indices.at(i + 1), mesh.indices.at(i + 2)};
        const bool up[3] = {above(v[0]), above(v[1]), above(v[2])};
        if (up[0] == up[1] && up[1] == up[2])
        {
            continue;
        }

        // Exactly two edges change side.
        PointKey ends[2];
        int count = 0;
        for (int e = 0; e < 3 && count < 2; ++e)
        {
            const int n = (e + 1) % 3;
            if (up[e] != up[n])
            {
                ends[count++] = cutPoint(v[e], v[n]);
            }
        }

        if (count == 2 && ends[0] != ends[1])
        {
            neighbours[ends[0]].append(ends[1]);
            neighbours[ends[1]].append(ends[0]);
        }
    }

    // Chain the segments into polylines. Start open chains at their ends so they come out whole.
    QVector<Contour> contours;
    QSet<PointKey> visited;

    const auto walk = [&](PointKey start)
    {
        Contour contour;
        PointKey previous = start;
        PointKey current = start;
        visited.insert(start);
        contour.points.append(position.value(start));

        forever
        {
            PointKey next = current;
            for (const PointKey candidate : neighbours.value(current))
            {
                if (candidate != previous && !visited.contains(candidate))
                {
                    next = candidate;
                    break;
                }
            }

            if (next == current)
            {
                contour.closed = contour.points.size() > 2 && neighbours.value(current).contains(start);
                break;
            }

            visited.insert(next);
            contour.points.append(position.value(next));
            previous = current;
            current = next;
        }
        contours.append(contour);
    };

    for (auto it = neighbours.constBegin(); it != neighbours.constEnd(); ++it)
    {
        if (it.value().size() == 1 && !visited.contains(it.key()))
        {
            walk(it.key());
        }
    }
    for (auto it = neighbours.constBegin(); it != neighbours.constEnd(); ++it)
    {
        if (!visited.contains(it.key()))
        {
            walk(it.key());
        }
    }

    return contours;
}

//---------------------------------------------------------------------------------------------------------------------
double MeshSlicer::girthAtHeight(const Mesh &mesh, float height)
{
    return longestClosedLength(sliceAtHeight(mesh, height));
}

//---------------------------------------------------------------------------------------------------------------------
double MeshSlicer::longestClosedLength(const QVector<Contour> &contours)
{
    double girth = 0;
    for (const Contour &contour : contours)
    {
        if (contour.closed)
        {
            girth = qMax(girth, contour.length());
        }
    }
    return girth;
}

} // namespace Seamly3D
