/******************************************************************************
 *   @file   piecemesher.h
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

#ifndef SEAMLY3D_PIECEMESHER_H
#define SEAMLY3D_PIECEMESHER_H

#include <QPolygonF>
#include <QString>
#include <QVector>

#include "mesh.h"

namespace Seamly3D
{

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief A pattern piece as the 3D workspace sees it, in centimeters, in the pattern's coordinates (Y down).
 */
struct PatternPiece
{
    quint32   id{0};
    QString   name{};
    QPolygonF seamLine{};   ///< The piece's outline along the seam line (the sewing line)
    int       quantity{1};  ///< How many to cut
    bool      onFold{false};
    QVector<int> corners{}; ///< Indices in seamLine of the piece's own points (where its seam lines meet)
    QVector<int> notches{}; ///< Indices in seamLine of its notches (where seams are lined up)
};

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Turns a piece outline into a flat triangle mesh with no edge longer than a given length, so the piece can be
 * bent around a body (and, later, simulated as cloth).
 *
 * The outline is resampled to the edge length, triangulated, and long triangle edges are split until all are short
 * enough (longest edge bisection, which keeps the mesh conforming). The mesh lies in the XY plane in the outline's
 * coordinates.
 */
class PieceMesher
{
public:
    /**
     * @brief mesh builds the piece mesh. Its first vertices are the outline, in order.
     * @param outlineVertex receives, for each point of @p outline, the mesh vertex it became.
     */
    static Mesh mesh(const QPolygonF &outline, double maxEdge, QVector<int> *outlineVertex = nullptr);

    /**
     * @brief resample returns the closed outline with extra points so that no edge is longer than @p maxEdge.
     * @param outlineIndex receives, for each point of @p outline, its index in the result (repeated points share one).
     */
    static QPolygonF resample(const QPolygonF &outline, double maxEdge, QVector<int> *outlineIndex = nullptr);

    /**
     * @brief turningAngle is how much the closed @p outline turns at its point @p index, in degrees: between the
     * directions to the points @p window before and after it along the outline (so a dense outline's small steps
     * along a curve do not count, a corner does).
     */
    static double turningAngle(const QPolygonF &outline, int index, double window);

    /**
     * @brief sides keeps the corners (indices in @p outline) where the outline turns more than @p minAngle: the
     * points between two sides. Points along a side (a notch, where a line runs into a curve) are dropped. Sharp
     * points closer than @p merge along the outline (a rounded corner traced point by point, a small step) are one
     * corner, at the sharpest of them. When none is sharp, the sharpest one stays.
     */
    static QVector<int> sides(const QPolygonF &outline, const QVector<int> &corners, double minAngle = 25.0,
                              double window = 1.0, double merge = 1.5);
};

} // namespace Seamly3D

#endif // SEAMLY3D_PIECEMESHER_H
