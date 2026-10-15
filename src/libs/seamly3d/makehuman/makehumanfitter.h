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
#include <QSharedPointer>
#include <QString>
#include <QVector>

#include "../bodymeasurements.h"
#include "../mesh.h"
#include "makehumanmodel.h"

namespace Seamly3D
{

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
        bool            fitted{false}; ///< false: the body has no control for this measurement yet
    };

    bool            valid{false};
    QString         errorString{};
    Mesh            mesh{};            ///< centimeters, feet at y = 0, facing +Z
    MakeHumanMacros macros{};
    double          scale{1.0};        ///< uniform scale applied on top of the height macro
    QVector<Entry>  entries{};

    /** @brief worst absolute difference between target and achieved over the fitted measurements. */
    double maxError() const;
    QVector<BodyMeasurement> unfitted() const;
};

/**
 * @brief Fits a MakeHuman body to a person's measurements.
 *
 * 1. Gender and muscle come from the measurements; age is adult (25 years).
 * 2. The height macro, then the weight macro (overall build from bust, waist and hip) and, for female bodies, the
 *    breast size macro (from bust minus lowbust) are searched by bisection.
 * 3. A uniform scale makes the height exact.
 * 4. The measure modifiers are solved together with Newton steps until every girth measured along MakeHuman's tape
 *    paths matches the target. MakeHuman's measurement plugin bisects one modifier at a time; solving them together
 *    is needed because neighbouring modifiers (bust and lowbust) change each other's measures. They may go past
 *    MakeHuman's slider range (see MakeHumanBody::modifierLimit).
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
