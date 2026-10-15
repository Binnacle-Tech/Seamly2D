/******************************************************************************
 *   @file   mesh.h
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

#ifndef SEAMLY3D_MESH_H
#define SEAMLY3D_MESH_H

#include <QVector>
#include <QVector3D>
#include <QtGlobal>

namespace Seamly3D
{

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Axis aligned bounding box of a set of 3D points.
 */
struct Bounds3D
{
    QVector3D minimum{};
    QVector3D maximum{};
    bool      valid{false};

    void      expand(const QVector3D &point);
    QVector3D center() const;
    QVector3D size() const;
    float     radius() const;
};

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Indexed triangle mesh used by the 3D workspace (avatars, pattern pieces, garments).
 *
 * Units are whatever the source data uses; the workspace treats them as scene units.
 */
class Mesh
{
public:
    Mesh() = default;

    QVector<QVector3D> positions{};
    QVector<QVector3D> normals{};  ///< Per vertex normals, same size as positions or empty.
    QVector<quint32>   indices{};  ///< Three indices per triangle.

    bool     isEmpty() const;
    bool     isValid() const;
    int      vertexCount() const;
    int      triangleCount() const;
    Bounds3D bounds() const;

    void     clear();
    void     computeNormals();
    void     append(const Mesh &other);
    void     translate(const QVector3D &offset);
    void     scale(float factor); ///< Uniform scale, factor must be positive.

    /** @brief Interleaved position (xyz) + normal (xyz) data ready for upload to a GPU buffer. */
    QVector<float> interleavedVertexData() const;
};

} // namespace Seamly3D

#endif // SEAMLY3D_MESH_H
