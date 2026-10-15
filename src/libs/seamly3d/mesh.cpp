/******************************************************************************
 *   @file   mesh.cpp
 **  @author Seamly2D project
 **  @date   15 10, 2026
 **
 **  @brief  Indexed triangle mesh and bounding box used by the 3D workspace.
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

#include "mesh.h"

#include <QtMath>

namespace Seamly3D
{

//---------------------------------------------------------------------------------------------------------------------
void Bounds3D::expand(const QVector3D &point)
{
    if (!valid)
    {
        minimum = point;
        maximum = point;
        valid = true;
        return;
    }

    minimum = QVector3D(qMin(minimum.x(), point.x()), qMin(minimum.y(), point.y()), qMin(minimum.z(), point.z()));
    maximum = QVector3D(qMax(maximum.x(), point.x()), qMax(maximum.y(), point.y()), qMax(maximum.z(), point.z()));
}

//---------------------------------------------------------------------------------------------------------------------
QVector3D Bounds3D::center() const
{
    return valid ? (minimum + maximum) * 0.5f : QVector3D();
}

//---------------------------------------------------------------------------------------------------------------------
QVector3D Bounds3D::size() const
{
    return valid ? maximum - minimum : QVector3D();
}

//---------------------------------------------------------------------------------------------------------------------
float Bounds3D::radius() const
{
    return valid ? size().length() * 0.5f : 0.0f;
}

//---------------------------------------------------------------------------------------------------------------------
bool Mesh::isEmpty() const
{
    return positions.isEmpty() || indices.isEmpty();
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief isValid checks that every index references an existing vertex and that normals match positions.
 */
bool Mesh::isValid() const
{
    if (indices.size() % 3 != 0)
    {
        return false;
    }

    if (!normals.isEmpty() && normals.size() != positions.size())
    {
        return false;
    }

    const auto count = static_cast<quint32>(positions.size());
    for (const quint32 index : indices)
    {
        if (index >= count)
        {
            return false;
        }
    }
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
int Mesh::vertexCount() const
{
    return static_cast<int>(positions.size());
}

//---------------------------------------------------------------------------------------------------------------------
int Mesh::triangleCount() const
{
    return static_cast<int>(indices.size() / 3);
}

//---------------------------------------------------------------------------------------------------------------------
Bounds3D Mesh::bounds() const
{
    Bounds3D box;
    for (const QVector3D &point : positions)
    {
        box.expand(point);
    }
    return box;
}

//---------------------------------------------------------------------------------------------------------------------
void Mesh::clear()
{
    positions.clear();
    normals.clear();
    indices.clear();
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief computeNormals calculates smooth per vertex normals as the area weighted sum of adjacent face normals.
 */
void Mesh::computeNormals()
{
    normals = QVector<QVector3D>(positions.size(), QVector3D());

    for (int i = 0; i + 2 < indices.size(); i += 3)
    {
        const quint32 a = indices.at(i);
        const quint32 b = indices.at(i + 1);
        const quint32 c = indices.at(i + 2);

        // Not normalized on purpose: the cross product length weights the face by its area.
        const QVector3D &pa = positions.at(static_cast<int>(a));
        const QVector3D faceNormal = QVector3D::crossProduct(positions.at(static_cast<int>(b)) - pa,
                                                             positions.at(static_cast<int>(c)) - pa);
        normals[static_cast<int>(a)] += faceNormal;
        normals[static_cast<int>(b)] += faceNormal;
        normals[static_cast<int>(c)] += faceNormal;
    }

    for (QVector3D &normal : normals)
    {
        normal = normal.isNull() ? QVector3D(0, 0, 1) : normal.normalized();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void Mesh::append(const Mesh &other)
{
    const bool keepNormals = (normals.size() == positions.size()) && (other.normals.size() == other.positions.size());
    const auto offset = static_cast<quint32>(positions.size());

    positions += other.positions;
    if (keepNormals)
    {
        normals += other.normals;
    }
    else
    {
        normals.clear();
    }

    indices.reserve(indices.size() + other.indices.size());
    for (const quint32 index : other.indices)
    {
        indices.append(index + offset);
    }

    if (!keepNormals)
    {
        computeNormals();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void Mesh::translate(const QVector3D &offset)
{
    for (QVector3D &point : positions)
    {
        point += offset;
    }
}

//---------------------------------------------------------------------------------------------------------------------
void Mesh::scale(float factor)
{
    // A positive uniform scale keeps normals and winding unchanged.
    Q_ASSERT(factor > 0.0f);
    for (QVector3D &point : positions)
    {
        point *= factor;
    }
}

//---------------------------------------------------------------------------------------------------------------------
QVector<float> Mesh::interleavedVertexData() const
{
    const bool hasNormals = normals.size() == positions.size();

    QVector<float> data;
    data.reserve(positions.size() * 6);
    for (int i = 0; i < positions.size(); ++i)
    {
        const QVector3D &p = positions.at(i);
        const QVector3D n = hasNormals ? normals.at(i) : QVector3D(0, 0, 1);
        data << p.x() << p.y() << p.z() << n.x() << n.y() << n.z();
    }
    return data;
}

} // namespace Seamly3D
