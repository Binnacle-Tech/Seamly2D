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
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QVector>

#include "bodymeasurements.h"
#include "mesh.h"
#include "piecemesher.h"

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
    LowerFront,    ///< From the waist down, around the hips (skirts)
    LowerBack,
    LeftLegFront,  ///< From the waist down the leg (trousers, shorts)
    LeftLegBack,
    RightLegFront,
    RightLegBack,
    Waistband,     ///< Around the waist, sewn to the top of a skirt or trousers
    Collar,        ///< Around the neck, sewn to the neckline
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

    float waistHeight{103.5f};     ///< where the top of skirts and trousers goes
    float hipHeight{85.5f};
    float crotchHeight{76.0f};
    float kneeHeight{47.0f};
    float ankleHeight{8.0f};
    float hipRadius{20.0f};        ///< radius of the cylinder skirts wrap around (hips plus clearance)
    float thighRadius{9.0f};       ///< leg girths as radii, at the crotch, knee and ankle
    float kneeRadius{6.0f};
    float ankleRadius{3.5f};
    QVector3D leftHip{9.0f, 76.0f, 0.0f};  ///< Left leg axis at the crotch
    QVector3D leftKnee{9.5f, 47.0f, 0.0f}; ///< Left leg axis at the knee
    float neckBaseHeight{140.5f};  ///< where collars go: the back of the neck
    float neckRadius{5.7f};        ///< neck and waist girths as radii, around their centers
    float waistRadius{12.0f};
    QVector3D neckCenter{0.0f, 140.5f, 0.0f};
    QVector3D waistCenter{0.0f, 103.5f, 0.0f};

    /** @brief fromMeasurements estimates the frame; the arm is assumed in an A-pose, about 40 degrees out. */
    static BodyFrame fromMeasurements(const BodyMeasurements &measurements);

    /** @brief withArm uses the arm of the body actually shown (its pose differs from body to body). */
    BodyFrame withArm(const QVector3D &shoulder, const QVector3D &wrist) const;

    /** @brief withLegs finds the legs of the body shown, by cutting it at the thigh and the knee. */
    BodyFrame withLegs(const Mesh &body) const;

    /** @brief withNeckAndWaist finds the neck and the waist of the body shown, by cutting it there. */
    BodyFrame withNeckAndWaist(const Mesh &body) const;

    /** @brief legAxis returns the left leg axis at @p height. */
    QVector3D legAxis(float height) const;
    /** @brief legRadius returns the left leg radius at @p height, from the thigh to the ankle. */
    float     legRadius(float height) const;
};

/**
 * @brief Moves a piece away from where its placement puts it, by hand. Zero leaves it there.
 *
 * Lift and around are along and about the axis of the piece's place: the body's for the torso, skirts, bands and
 * pieces beside the body (up, and around seen from above), the arm's for sleeves and the leg's for trouser legs (up
 * toward the shoulder or the hip, and around the limb). A mirrored copy moves the mirrored way, so pairs stay
 * symmetric. Turn spins the flat piece in its own plane before it is placed, for pieces drawn sideways.
 */
struct PieceAdjustment
{
    float lift{0.0f};   ///< centimeters, up
    float around{0.0f}; ///< degrees
    float turn{0.0f};   ///< degrees, counterclockwise as drawn

    bool isNull() const { return qFuzzyIsNull(lift) && qFuzzyIsNull(around) && qFuzzyIsNull(turn); }
};

/**
 * @brief Places flat piece meshes around the body: the starting shape that sewing and draping begin from.
 *
 * Torso pieces are wrapped around a cylinder just outside the body, sleeves around cylinders beside the arms; the
 * pattern reads the right way round when seen from outside. Wrapping bends without stretching, so piece sizes stay
 * true. A piece cut twice (or on the fold) gets a mirrored copy: around the center line for front and back, on the
 * other side for sides, sleeves and legs.
 *
 * Skirts wrap around the hips from the waist down. A trouser leg piece wraps around its leg with the middle of its
 * hem (where the crease runs) in front of the leg, or behind it, and its upper part blends into the hip cylinder so
 * the crotch curve reaches the center line. The side seam goes to the outside of the leg: it is the side the top of
 * the piece leans to, the crotch extension sticking out on the other side.
 *
 * Waistbands and collars are rings around the waist or the neck, lengthwise, their lower edge where the top of the
 * garment (or its neckline) is. A band cut once goes all the way round, its ends at the center front; a band cut
 * twice or on the fold is half a ring from the center back to the center front, its mirrored copy the other half.
 */
class PieceArrangement
{
    Q_DECLARE_TR_FUNCTIONS(Seamly3D::PieceArrangement)

public:
    /**
     * @brief guess picks a placement from the piece name ("Front bodice", "Sleeve", "Rückenteil"...); a name that
     * says nothing ("Piece 7", as imported from a DXF file without names) goes by @p outline instead, when given.
     */
    static PiecePlacement guess(const QString &name, const QPolygonF &outline = QPolygonF());

    /**
     * @brief guessFromShape picks a placement from the outline alone (centimeters): small pieces (pockets, flaps,
     * cuffs, yokes) and long upright strips (facings) beside the body, long flat strips as waistbands (50 cm and
     * more) or collars, pieces with a domed top in the middle as sleeves, very long ones (80 cm and more) as trouser
     * legs, the rest on the front.
     */
    static PiecePlacement guessFromShape(const QPolygonF &outline);

    /**
     * @brief neckDepth is how far the neckline of a bodice piece drawn upright dips at the center (centimeters): from
     * its highest point (the neck side) down to the top of the side nearest to it (the center front or back). A front
     * neck is much deeper than a back neck.
     */
    static double neckDepth(const QPolygonF &outline);

    /** @brief isYoke tells a yoke by its name: it goes on top of the front or back, the other pieces there below it. */
    static bool isYoke(const QString &name);

    /**
     * @brief isUnderSleeve tells the under sleeve of a two-piece sleeve by its name: it goes round the inside of the
     * arm, below the top sleeve.
     */
    static bool isUnderSleeve(const QString &name);

    /**
     * @brief centerFirst orders pieces sharing a place from the center out: cut on the fold (0), named the center
     * or inside one (1), nothing said (2), named side or outside (3).
     */
    static int  centerFirst(const PatternPiece &piece);

    static QString displayName(PiecePlacement placement);

    /** @brief placementId names a placement in files ("front", "left-leg-back"...): it never changes. */
    static QString        placementId(PiecePlacement placement);
    static PiecePlacement placementFromId(const QString &id, bool *known = nullptr);

    /**
     * @brief place returns the piece and, when @p pair is set, its mirrored copy, wrapped around the body.
     * @param flat the piece mesh in pattern coordinates (centimeters, Y down, Z = 0)
     * @param bounds the piece outline's bounding box
     * @param flip mirror the piece (when the guess put its center line on the wrong side)
     * @param asideStart where along the row of pieces hung beside the body this one starts (centimeters)
     * @param adjustment moved by hand from there
     */
    static QVector<Mesh> place(const Mesh &flat, const QRectF &bounds, PiecePlacement placement, bool pair, bool flip,
                               const BodyFrame &frame, float asideStart = 0,
                               const PieceAdjustment &adjustment = PieceAdjustment());

    /** @brief aroundRadius is the radius a piece goes around at in its place, to turn distances into angles. */
    static float aroundRadius(PiecePlacement placement, const BodyFrame &frame);

    /**
     * @brief axis is what a piece in @p placement is lifted along and turned about: a point on it, and up along it.
     * @param side +1 for a piece left of the center (the left arm or leg), -1 for one right of it
     */
    static void axis(PiecePlacement placement, const BodyFrame &frame, float side, QVector3D *origin, QVector3D *up);

private:
    static QVector<Mesh> placeAsDrawn(const Mesh &flat, const QRectF &bounds, PiecePlacement placement, bool pair,
                                      bool flip, const BodyFrame &frame, float asideStart);
};

} // namespace Seamly3D

#endif // SEAMLY3D_PIECEARRANGEMENT_H
