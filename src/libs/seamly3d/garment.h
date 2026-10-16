/******************************************************************************
 *   @file   garment.h
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

#ifndef SEAMLY3D_GARMENT_H
#define SEAMLY3D_GARMENT_H

#include <QColor>
#include <QString>
#include <QVector>
#include <QVector3D>

#include "mesh.h"

namespace Seamly3D
{

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief One cut piece of fabric in 3D: a pattern piece, or the mirrored copy of one.
 *
 * Both meshes share their vertices: rest is the flat piece (centimeters, the pattern's coordinates, Z = 0), shape is
 * where it is in 3D. The first outlineCount vertices are the outline, in order. Corners are the piece's own points
 * on the outline where it turns (see PieceMesher::sides); between two corners runs an edge: a whole side of the
 * piece (a seam line, a hem, a fold...).
 */
struct GarmentPanel
{
    quint32      pieceId{0};
    int          copy{0};          ///< 0 for the piece, 1 for its mirrored copy
    QString      name{};
    QColor       color{};
    bool         sewable{true};    ///< false for pieces hung beside the body
    Mesh         rest{};
    Mesh         shape{};
    int          outlineCount{0};
    QVector<int> corners{};        ///< Outline vertices, increasing

    int          edgeCount() const;
    /** @brief edgeVertices returns the outline vertices from corner @p edge to the next corner, both included. */
    QVector<int> edgeVertices(int edge) const;
    double       edgeLength(int edge) const; ///< along the flat piece, centimeters
};

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Two edges sewn together. With reversed set, the start of edge A meets the end of edge B.
 *
 * A seam may take only part of edge A, from startA to endA (shares of its length): a sleeve cap is sewn to the front
 * armhole with one part and to the back armhole with the other.
 */
struct Seam
{
    int    panelA{0};
    int    edgeA{0};
    int    panelB{0};
    int    edgeB{0};
    bool   reversed{false};
    double startA{0.0};
    double endA{1.0};
};

/**
 * @brief Finds the seams of a garment from its arranged pieces.
 *
 * Two edges are sewn together when they have the same length (within 5%) and the same shape (one bulges where the
 * other curves in, or both are straight), are on the same side of the body and lie close together once the pieces
 * are arranged: side seams, shoulder seams, the center line of a piece cut on the fold with its mirrored copy, the
 * underarm seam of a sleeve. Armholes and necklines are left open: sleeves and collars are eased into them. Two
 * neighbouring edges of the same length meeting at a sharp point are a dart and are sewn closed. Hems (the bottom edge
 * of a piece) are never sewn.
 *
 * Then sleeves are set in: an edge left over that bulges out (a sleeve cap) is sewn to two edges left over that curve
 * in (the front and back armholes) on the same side of the body, when it is as long as both together or up to 15%
 * longer (the ease). The cap's ends meet at the underarm, its middle at the shoulder.
 */
class SeamFinder
{
public:
    static QVector<Seam> find(const QVector<GarmentPanel> &panels);

    static constexpr double lengthTolerance = 0.05; ///< relative
    static constexpr double maxDistance = 35.0;     ///< centimeters between the edges once arranged
};

} // namespace Seamly3D

#endif // SEAMLY3D_GARMENT_H
