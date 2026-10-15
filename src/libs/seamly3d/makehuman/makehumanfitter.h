/******************************************************************************
 *   @file   makehumanfitter.h
 **  @brief  Fits a MakeHuman body to body measurements.
 **
 **  Ported to C++ for Seamly2D from MakeHuman 1.x
 **  (https://github.com/makehumancommunity/makehuman), the bisection of plugins/0_modeling_a_measurement.py (MeasurementValueConverter).
 **
 **  @copyright
 **  Copyright (C) 2001-2020  MakeHuman Team (www.makehumancommunity.org)
 **  Copyright (C) 2026 Seamly2D project (C++ port and changes)
 **
 **  This program is free software: you can redistribute it and/or modify
 **  it under the terms of the GNU Affero General Public License as
 **  published by the Free Software Foundation, either version 3 of the
 **  License, or (at your option) any later version.
 **
 **  This program is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU Affero General Public License for more details.
 **
 **  You should have received a copy of the GNU Affero General Public License
 **  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **
 **  It is combined with the GPL-3.0 parts of Seamly2D as permitted by
 **  section 13 of both licenses.
 **
 *****************************************************************************/

#ifndef SEAMLY3D_MAKEHUMANFITTER_H
#define SEAMLY3D_MAKEHUMANFITTER_H

#include <QCoreApplication>
#include <QSet>
#include <QSharedPointer>
#include <QString>
#include <QVector>

#include "../bodymeasurements.h"
#include "../mesh.h"
#include "makehumanmodel.h"

namespace Seamly3D
{

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Takes every body measurement of a MakeHuman body, the way the Seamly2D measurement database defines them.
 *
 * - girths and arm length: along MakeHuman's tape paths (rulers), except where Seamly2D defines them elsewhere:
 *   the neck at its base (a path touching the nape, the neck sides and the front of the neck base) and the upper
 *   arm at armpit level (a cut square to the upper arm)
 * - heights: from the floor to landmarks found on the body (nape, shoulder tip, armpit, crotch...), the sides of the
 *   waist and hip loops, the levels of the knee, calf and ankle loops, and the most forward point of the bust
 * - shoulder width: along the shortest path over the back from one shoulder tip to the other
 * - head circumference: a level cut through the widest part of the head
 * - elbow circumference: a cut square to the forearm through the elbow
 *
 * Only the few thousand vertices these need are evaluated, so measuring is cheap enough to run inside the fitter.
 */
class MakeHumanTape
{
public:
    explicit MakeHumanTape(const MakeHumanData &data);

    /**
     * @brief measure returns every BodyMeasurement (indexed by its enum value) in centimeters, with the body scaled
     * uniformly so its height is @p height.
     * @param scale receives the scale used.
     */
    QVector<double> measure(const MakeHumanBody &body, double height, double *scale = nullptr) const;

    QVector<int> shoulderPath() const; ///< Vertices of the path over the back between the shoulder tips.
    QVector<int> neckBasePath() const; ///< Closed path around the base of the neck (first vertex repeated last).

    /**
     * @brief lockLandmarks picks the bust point and the waist and hip side vertices on @p body and keeps them.
     * Re-picking them on every measure makes the measures jump from one vertex to the next, which a solver can not
     * follow; once the body has roughly the right shape the same vertices stay the right ones.
     */
    void lockLandmarks(const MakeHumanBody &body);

private:
    int bustPoint(const QVector<QVector3D> &p) const;
    int sideVertex(const QVector<QVector3D> &p, const QVector<int> &loop) const;

    QVector<int> m_probe{};          ///< Every vertex a measurement reads
    QVector<int> m_floor{};
    QVector<int> m_top{};
    QVector<int> m_bust{};
    QVector<int> m_shoulderPath{};
    QVector<quint32> m_headTriangles{};  ///< Head triangles near the level of its widest part
    QVector<quint32> m_elbowTriangles{}; ///< Left arm triangles around the elbow
    QVector<quint32> m_upperArmTriangles{}; ///< Left arm triangles around the armpit level
    QVector<int>     m_neckBasePath{};
    QSet<int>        m_neckCorners{};
    int          m_armpitMirror{-1};
    int          m_shoulderMirror{-1};
    int          m_lockedBustPoint{-1};
    int          m_lockedWaistSide{-1};
    int          m_lockedHipSide{-1};
    mutable QVector<QVector3D> m_positions{}; ///< Probe positions (other vertices are not updated)
};

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Result of fitting a MakeHuman body to body measurements.
 */
struct MakeHumanFit
{
    struct Entry
    {
        BodyMeasurement measurement{BodyMeasurement::Height};
        double          target{0};   ///< centimeters
        double          achieved{0}; ///< centimeters, measured on the fitted body
        bool            fitted{false}; ///< false: not used for this body (lowbust and bustpoint of a child)
    };

    bool            valid{false};
    QString         errorString{};
    Mesh            mesh{};            ///< centimeters, feet at y = 0, facing +Z
    MakeHumanMacros macros{};
    QVector<double> modifiers{};       ///< in MakeHumanData::modifierNames() order
    double          scale{1.0};        ///< uniform scale applied on top of the height macro
    QVector<Entry>  entries{};

    /** @brief worst absolute difference between target and achieved over the fitted measurements. */
    double maxError() const;
    QVector<BodyMeasurement> unfitted() const;
};

/**
 * @brief Fits a MakeHuman body to a person's measurements.
 *
 * 1. Gender, age and muscle come from the measurements. Children (under 13) get a flat chest, and their lowbust
 *    and bustpoint are not fitted.
 * 2. The height macro, then the weight macro (overall build from bust, waist and hip) and, for female bodies, the
 *    breast size macro (from bust minus lowbust) are found by bisection.
 * 3. A uniform scale makes the height exact.
 * 4. All modifiers (MakeHuman's measure modifiers and the modeling targets for lengths and landmark heights) are
 *    solved together by damped least squares (Levenberg-Marquardt) so every measurement taken by MakeHumanTape
 *    matches. MakeHuman's own measurement plugin bisects one modifier at a time, which can not handle modifiers
 *    that affect each other's measures (bust and lowbust, leg lengths and every height above them).
 */
class MakeHumanFitter
{
    Q_DECLARE_TR_FUNCTIONS(Seamly3D::MakeHumanFitter)

public:
    static MakeHumanFit fit(const BodyMeasurements &measurements,
                            const QSharedPointer<const MakeHumanData> &data = MakeHumanData::instance());

    static constexpr double tolerance = 0.05; ///< centimeters
};

} // namespace Seamly3D

#endif // SEAMLY3D_MAKEHUMANFITTER_H
