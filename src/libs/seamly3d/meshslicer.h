/******************************************************************************
 *   @file   meshslicer.h
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

#ifndef SEAMLY3D_MESHSLICER_H
#define SEAMLY3D_MESHSLICER_H

#include <QVector>
#include <QVector3D>

#include "mesh.h"

namespace Seamly3D
{

/**
 * @brief Cuts meshes with horizontal planes. This is how girths are measured on a body: the length of the closed loop
 * where the plane y = height cuts the surface, exactly like a tape measure held level.
 */
class MeshSlicer
{
public:
    /** @brief A polyline where the plane cuts the mesh. Closed loops do not repeat the first point. */
    struct Contour
    {
        QVector<QVector3D> points{};
        bool               closed{false};

        double length() const;
    };

    static QVector<Contour> sliceAtHeight(const Mesh &mesh, float height);

    /** @brief girthAtHeight returns the length of the longest closed loop at @p height, or 0 if there is none. */
    static double girthAtHeight(const Mesh &mesh, float height);
};

} // namespace Seamly3D

#endif // SEAMLY3D_MESHSLICER_H
