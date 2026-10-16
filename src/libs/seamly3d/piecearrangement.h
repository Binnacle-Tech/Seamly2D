/******************************************************************************
 *   @file   piecearrangement.h
 **  @author Seamly2D project
 **  @date   15 10, 2026
 **
 **  @brief  Placement of pattern pieces around the body.
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

#ifndef SEAMLY3D_PIECEARRANGEMENT_H
#define SEAMLY3D_PIECEARRANGEMENT_H

#include <QCoreApplication>
#include <QRectF>
#include <QString>
#include <QVector>

#include "bodymeasurements.h"
#include "mesh.h"

namespace Seamly3D
{

/**
 * @brief Where a pattern piece starts around the body, before sewing and draping.
 */
enum class PiecePlacement : int
{
    Front,
    Back,
    LeftSide,
    RightSide,
    LeftSleeve,
    RightSleeve,
    Aside,     ///< Hung flat behind the body (collars, pockets, facings...)
    Hidden,
    Count
};

/**
 * @brief The body sizes the arrangement wraps pieces around, in centimeters (Y up, body facing +Z, left at +X).
 */
struct BodyFrame
{
    float neckHeight{144.0f};      ///< where the top of torso pieces goes (a little above the neck and shoulders)
    float shoulderHeight{137.0f};  ///< where the top of sleeves goes
    float torsoRadius{17.0f};      ///< radius of the cylinder torso pieces wrap around
    float sleeveRadius{7.0f};      ///< radius of the cylinder sleeves wrap around (arm plus clearance)
    float armRadius{4.5f};
    QVector3D leftShoulder{18.0f, 135.0f, 0.0f}; ///< Left arm axis at the shoulder tip; the right arm mirrors it
    QVector3D leftWrist{50.0f, 95.0f, 0.0f};     ///< Left arm axis at the wrist

    /** @brief fromMeasurements estimates the frame; the arm is assumed in an A-pose, about 40 degrees out. */
    static BodyFrame fromMeasurements(const BodyMeasurements &measurements);

    /** @brief withArm uses the arm of the body actually shown (its pose differs from body to body). */
    BodyFrame withArm(const QVector3D &shoulder, const QVector3D &wrist) const;
};

/**
 * @brief Places flat piece meshes around the body: the starting shape that sewing and draping begin from.
 *
 * Torso pieces are wrapped around a cylinder just outside the body, sleeves around cylinders beside the arms; the
 * pattern reads the right way round when seen from outside. Wrapping bends without stretching, so piece sizes stay
 * true. A piece cut twice (or on the fold) gets a mirrored copy: around the center line for front and back, on the
 * other side for sides and sleeves.
 */
class PieceArrangement
{
    Q_DECLARE_TR_FUNCTIONS(Seamly3D::PieceArrangement)

public:
    /** @brief guess picks a placement from the piece name ("Front bodice", "Sleeve", "Rückenteil"...). */
    static PiecePlacement guess(const QString &name);

    static QString displayName(PiecePlacement placement);

    /**
     * @brief place returns the piece and, when @p pair is set, its mirrored copy, wrapped around the body.
     * @param flat the piece mesh in pattern coordinates (centimeters, Y down, Z = 0)
     * @param bounds the piece outline's bounding box
     * @param flip mirror the piece (when the guess put its center line on the wrong side)
     * @param asideStart where along the row of pieces hung beside the body this one starts (centimeters)
     */
    static QVector<Mesh> place(const Mesh &flat, const QRectF &bounds, PiecePlacement placement, bool pair, bool flip,
                               const BodyFrame &frame, float asideStart = 0);
};

} // namespace Seamly3D

#endif // SEAMLY3D_PIECEARRANGEMENT_H
