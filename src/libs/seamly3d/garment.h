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
    bool         band{false};      ///< a waistband or collar: its lower edge is sewn along a waistline or neckline
    bool         sleeve{false};    ///< a sleeve, or one piece of it: its cap is set into an armhole
    Mesh         rest{};
    Mesh         shape{};
    int          outlineCount{0};
    QVector<int> corners{};        ///< Outline vertices, increasing
    QVector<int> notches{};        ///< Outline vertices with a notch, increasing

    int          edgeCount() const;
    /** @brief edgeVertices returns the outline vertices from corner @p edge to the next corner, both included. */
    QVector<int> edgeVertices(int edge) const;
    double       edgeLength(int edge) const; ///< along the flat piece, centimeters
    /** @brief edgeNotches returns where the notches on an edge are along it: shares of its length, increasing. */
    QVector<double> edgeNotches(int edge) const;
};

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Two edges sewn together. With reversed set, the start of edge A meets the end of edge B.
 *
 * A seam may take only part of edge A, from startA to endA (shares of its length): a sleeve cap is sewn to the front
 * armhole with one part and to the back armhole with the other. Likewise part of edge B, from startB to endB.
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
    double startB{0.0}; ///< the part of edge B sewn, in its own direction (whatever reversed says)
    double endB{1.0};
};

/**
 * @brief Finds the seams of a garment from its arranged pieces.
 *
 * Two edges are sewn together when they have the same length (within 5%) and the same shape (one bulges where the
 * other curves in, both are straight, or both bulge out alike), are on the same side of the body and lie close
 * together once the pieces are arranged: side seams, shoulder seams, the center line of a piece cut on the fold with
 * its mirrored copy, the underarm seam of a sleeve. Two edges that both curve in are sewn only when they already lie
 * together (the inseams and crotch seams of trousers): armholes and necklines are left open, as sleeves and collars
 * are eased into them. Two neighbouring edges of the same length meeting at a sharp point are a dart and are sewn
 * closed. Hems (the bottom edge of a piece) and waistlines (a straight top edge) are never sewn. Two edges that fan
 * out at a sharp corner from the end of another seam between their pieces (the front and back armholes from the end
 * of the shoulder seam) are two sides of an opening: of two such seams, the one that curves in more is left open.
 *
 * Then the body's yokes are sewn to the panels below them (as last, below), and sleeves are set in. An opening left
 * in the garment is followed edge by edge, across the seams found: the top of a sleeve (one cap, or a top sleeve's
 * cap and an under sleeve's), an armhole (in any number of pieces). The top of a sleeve of two pieces or more is sewn
 * into the armhole opening on its side that it is as long as or up to 15% longer than (the ease): the top of the cap
 * at the top of the armhole, the rest shared out evenly round both. A one-piece cap (an edge left over that bulges
 * out) is sewn to two edges left over that curve in (the front and back armholes) in the same way, its ends at the
 * underarm, its middle at the shoulder; or, when the armhole is in more pieces, as an opening.
 *
 * Last, waistbands and collars are sewn on: the lower long edge of the band along every edge left open within reach
 * of it and at its height (the waistlines of a skirt or trousers, the necklines of a bodice; from bandDrop below to
 * bandRise above), in order around the body, each edge taking
 * a share of the band as long as itself. Half a band takes only the edges on its side of the body. A band left over
 * (an under collar) is sewn round to the band it lies on (the upper collar), edge by edge.
 *
 * And an edge still open is sewn to the open edges of other pieces lying along it within partReach, when they are as
 * long as it together (within partsTolerance): a yoke's lower edge to the tops of the panels below it. Hems and
 * waistlines are open to this, since both of those are straight.
 */
class SeamFinder
{
public:
    static QVector<Seam> find(const QVector<GarmentPanel> &panels);

    static constexpr double lengthTolerance = 0.05; ///< relative
    static constexpr double maxDistance = 35.0;     ///< centimeters between the edges once arranged
    static constexpr double closeMiddles = 10.0;    ///< centimeters: two edges curving in this close are sewn
    static constexpr float  bandReach = 20.0f;      ///< centimeters from a band to the edges it is sewn to
    static constexpr float  bandDrop = 12.0f;       ///< centimeters those edges may go below the band's lower edge
    static constexpr float  bandRise = 5.0f;        ///< and above it
    static constexpr float  partReach = 5.0f;       ///< centimeters from an edge to the edges sewn along it
    static constexpr double partsTolerance = 0.08;  ///< relative: those edges together as long as it
};

} // namespace Seamly3D

#endif // SEAMLY3D_GARMENT_H
