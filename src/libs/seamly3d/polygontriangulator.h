/******************************************************************************
 *   @file   polygontriangulator.h
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

#ifndef SEAMLY3D_POLYGONTRIANGULATOR_H
#define SEAMLY3D_POLYGONTRIANGULATOR_H

#include <QPolygonF>
#include <QVector>
#include <QtGlobal>

#include "mesh.h"

namespace Seamly3D
{

/**
 * @brief Converts 2D pattern piece outlines into triangles so they can be placed and simulated in 3D.
 */
class PolygonTriangulator
{
public:
    /**
     * @brief triangulate splits a simple (non self-intersecting) polygon into triangles with ear clipping.
     *
     * A closing point equal to the first point is ignored. The returned indices reference the points of @p outline
     * and are always wound counter-clockwise in the outline's coordinate system.
     * @return three indices per triangle, or an empty vector if the outline is degenerate.
     */
    static QVector<quint32> triangulate(const QPolygonF &outline);

    /**
     * @brief flatPieceMesh builds a flat mesh of a piece outline lying in the XY plane at height @p z,
     * facing +Z.
     */
    static Mesh flatPieceMesh(const QPolygonF &outline, float z = 0.0f);

    /** @brief signedArea returns the polygon area, positive for counter-clockwise outlines. */
    static qreal signedArea(const QPolygonF &outline);
};

} // namespace Seamly3D

#endif // SEAMLY3D_POLYGONTRIANGULATOR_H
