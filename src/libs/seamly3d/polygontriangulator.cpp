/******************************************************************************
 *   @file   polygontriangulator.cpp
 **  @author Seamly2D project
 **  @date   15 10, 2026
 **
 **  @brief  Triangulation of 2D pattern piece outlines for use in 3D.
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

#include "polygontriangulator.h"

#include <QtMath>

namespace
{

//---------------------------------------------------------------------------------------------------------------------
QPolygonF openOutline(const QPolygonF &outline)
{
    QPolygonF points = outline;
    if (points.size() > 1 && points.constFirst() == points.constLast())
    {
        points.removeLast();
    }
    return points;
}

//---------------------------------------------------------------------------------------------------------------------
qreal cross(const QPointF &a, const QPointF &b, const QPointF &c)
{
    return (b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x());
}

//---------------------------------------------------------------------------------------------------------------------
bool pointInTriangle(const QPointF &p, const QPointF &a, const QPointF &b, const QPointF &c)
{
    // Triangle is counter-clockwise; points on an edge count as inside so touching vertices block the ear.
    return cross(a, b, p) >= 0 && cross(b, c, p) >= 0 && cross(c, a, p) >= 0;
}

} // anonymous namespace

namespace Seamly3D
{

//---------------------------------------------------------------------------------------------------------------------
qreal PolygonTriangulator::signedArea(const QPolygonF &outline)
{
    const QPolygonF points = openOutline(outline);
    qreal area = 0;
    for (int i = 0; i < points.size(); ++i)
    {
        const QPointF &p = points.at(i);
        const QPointF &q = points.at((i + 1) % points.size());
        area += p.x() * q.y() - q.x() * p.y();
    }
    return area * 0.5;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<quint32> PolygonTriangulator::triangulate(const QPolygonF &outline)
{
    const QPolygonF points = openOutline(outline);
    QVector<quint32> triangles;
    if (points.size() < 3)
    {
        return triangles;
    }

    const qreal area = signedArea(points);
    if (qFuzzyIsNull(area))
    {
        return triangles;
    }

    // Work on a counter-clockwise list of vertex indices.
    QVector<quint32> remaining;
    remaining.reserve(points.size());
    for (int i = 0; i < points.size(); ++i)
    {
        remaining.append(static_cast<quint32>(area > 0 ? i : points.size() - 1 - i));
    }

    const auto pointAt = [&points](quint32 index) { return points.at(static_cast<int>(index)); };

    // Each pass must clip at least one ear; otherwise the outline is not simple and we stop instead of looping.
    int guard = 2 * static_cast<int>(remaining.size());
    int i = 0;
    while (remaining.size() > 3 && guard-- > 0)
    {
        const int count = static_cast<int>(remaining.size());
        const quint32 prev = remaining.at((i + count - 1) % count);
        const quint32 curr = remaining.at(i % count);
        const quint32 next = remaining.at((i + 1) % count);

        const QPointF a = pointAt(prev);
        const QPointF b = pointAt(curr);
        const QPointF c = pointAt(next);

        bool isEar = cross(a, b, c) > 0; // convex corner
        for (int j = 0; isEar && j < count; ++j)
        {
            const quint32 other = remaining.at(j);
            if (other != prev && other != curr && other != next && pointInTriangle(pointAt(other), a, b, c))
            {
                isEar = false;
            }
        }

        if (isEar)
        {
            triangles << prev << curr << next;
            remaining.remove(i % count);
            guard = 2 * static_cast<int>(remaining.size());
            i = qMax(0, i - 1);
        }
        else
        {
            ++i;
        }
    }

    if (remaining.size() != 3)
    {
        // Ran out of ears: the outline intersects itself.
        return QVector<quint32>();
    }

    triangles << remaining.at(0) << remaining.at(1) << remaining.at(2);
    return triangles;
}

//---------------------------------------------------------------------------------------------------------------------
Mesh PolygonTriangulator::flatPieceMesh(const QPolygonF &outline, float z)
{
    Mesh mesh;
    const QPolygonF points = openOutline(outline);
    mesh.indices = triangulate(points);
    if (mesh.indices.isEmpty())
    {
        return Mesh();
    }

    mesh.positions.reserve(points.size());
    mesh.normals.reserve(points.size());
    for (const QPointF &point : points)
    {
        mesh.positions.append(QVector3D(static_cast<float>(point.x()), static_cast<float>(point.y()), z));
        mesh.normals.append(QVector3D(0, 0, 1));
    }
    return mesh;
}

} // namespace Seamly3D
