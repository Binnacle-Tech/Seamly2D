/******************************************************************************
 *   @file   parametricbody.h
 **  @author Seamly2D project
 **  @date   15 10, 2026
 **
 **  @brief  Body model generated from body measurements.
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

#ifndef SEAMLY3D_PARAMETRICBODY_H
#define SEAMLY3D_PARAMETRICBODY_H

#include "bodymeasurements.h"
#include "mesh.h"

namespace Seamly3D
{

/**
 * @brief A body generated from measurements, split in parts so each part can be measured or simulated on its own.
 *
 * Units are centimeters. The body stands on the XZ plane (feet at y = 0), Y is up, it faces +Z and its left side is
 * at +X.
 */
struct BodyModel
{
    BodyMeasurements measurements{}; ///< The sanitized measurements the body was built from.

    Mesh torso{};    ///< Trunk, neck and head.
    Mesh leftLeg{};
    Mesh rightLeg{};
    Mesh leftArm{};
    Mesh rightArm{};

    Mesh combined() const;
};

/**
 * @brief Builds a body model that fits a set of body measurements.
 *
 * The body is lofted through elliptical cross sections placed at the measured heights. Each girth section is scaled
 * so its perimeter equals the measured girth exactly, so a tape measure (MeshSlicer::girthAtHeight) taken on the model
 * at a landmark gives back the input measurement. Shapes between landmarks follow average body proportions.
 *
 * This is an original implementation; no third party body model is used.
 */
class ParametricBody
{
public:
    static BodyModel generate(const BodyMeasurements &measurements, int segments = 48);

    /** @brief Heights where the torso girths are placed (the measured landmarks). */
    static float underbustHeight(const BodyMeasurements &measurements);
    static float thighHeight(const BodyMeasurements &measurements);
};

} // namespace Seamly3D

#endif // SEAMLY3D_PARAMETRICBODY_H
