/******************************************************************************
 *   @file   orbitcamera.h
 **  @author Seamly2D project
 **  @date   15 10, 2026
 **
 **  @brief  Turntable camera of the 3D workspace viewport.
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

#ifndef SEAMLY3D_ORBITCAMERA_H
#define SEAMLY3D_ORBITCAMERA_H

#include <QMatrix4x4>
#include <QVector3D>

#include "mesh.h"

namespace Seamly3D
{

/**
 * @brief Turntable camera orbiting a target point. The world is Y-up; the ground is the XZ plane.
 *
 * Angles are in degrees. Yaw 0 looks along -Z (the camera sits on +Z), pitch is clamped to avoid flipping over the poles.
 */
class OrbitCamera
{
public:
    OrbitCamera();

    void       reset();
    void       frame(const Bounds3D &bounds);

    void       orbit(float deltaYaw, float deltaPitch);
    void       pan(float deltaX, float deltaY);
    void       zoom(float factor);

    QVector3D  target() const;
    float      distance() const;
    float      yaw() const;
    float      pitch() const;
    float      fieldOfView() const;

    void       setTarget(const QVector3D &target);
    void       setDistance(float distance);
    void       setAngles(float yaw, float pitch);

    QVector3D  eye() const;
    QMatrix4x4 viewMatrix() const;
    QMatrix4x4 projectionMatrix(float aspectRatio) const;

    static constexpr float minPitch = -89.0f;
    static constexpr float maxPitch = 89.0f;
    static constexpr float minDistance = 0.001f;

private:
    QVector3D m_target{};
    float     m_distance{5.0f};
    float     m_yaw{0.0f};
    float     m_pitch{15.0f};
    float     m_fieldOfView{45.0f};
};

} // namespace Seamly3D

#endif // SEAMLY3D_ORBITCAMERA_H
