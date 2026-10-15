/******************************************************************************
 *   @file   orbitcamera.cpp
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

#include "orbitcamera.h"

#include <QtMath>

#include <cmath>

namespace Seamly3D
{

//---------------------------------------------------------------------------------------------------------------------
OrbitCamera::OrbitCamera()
{
    reset();
}

//---------------------------------------------------------------------------------------------------------------------
void OrbitCamera::reset()
{
    m_target = QVector3D();
    m_distance = 5.0f;
    m_yaw = 0.0f;
    m_pitch = 15.0f;
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief frame moves the target to the center of @p bounds and backs off until the whole box fits the view.
 */
void OrbitCamera::frame(const Bounds3D &bounds)
{
    if (!bounds.valid)
    {
        return;
    }

    m_target = bounds.center();
    const float radius = qMax(bounds.radius(), minDistance);
    const float halfFov = qDegreesToRadians(m_fieldOfView * 0.5f);
    m_distance = radius / qSin(halfFov) * 1.1f;
}

//---------------------------------------------------------------------------------------------------------------------
void OrbitCamera::orbit(float deltaYaw, float deltaPitch)
{
    setAngles(m_yaw + deltaYaw, m_pitch + deltaPitch);
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief pan shifts the target in the view plane. Deltas are fractions of the visible height at the target.
 */
void OrbitCamera::pan(float deltaX, float deltaY)
{
    const QMatrix4x4 view = viewMatrix();
    const QVector3D right(view(0, 0), view(0, 1), view(0, 2));
    const QVector3D up(view(1, 0), view(1, 1), view(1, 2));

    const float visibleHeight = 2.0f * m_distance * qTan(qDegreesToRadians(m_fieldOfView * 0.5f));
    m_target += (-right * deltaX + up * deltaY) * visibleHeight;
}

//---------------------------------------------------------------------------------------------------------------------
void OrbitCamera::zoom(float factor)
{
    if (factor > 0.0f)
    {
        setDistance(m_distance * factor);
    }
}

//---------------------------------------------------------------------------------------------------------------------
QVector3D OrbitCamera::target() const
{
    return m_target;
}

//---------------------------------------------------------------------------------------------------------------------
float OrbitCamera::distance() const
{
    return m_distance;
}

//---------------------------------------------------------------------------------------------------------------------
float OrbitCamera::yaw() const
{
    return m_yaw;
}

//---------------------------------------------------------------------------------------------------------------------
float OrbitCamera::pitch() const
{
    return m_pitch;
}

//---------------------------------------------------------------------------------------------------------------------
float OrbitCamera::fieldOfView() const
{
    return m_fieldOfView;
}

//---------------------------------------------------------------------------------------------------------------------
void OrbitCamera::setTarget(const QVector3D &target)
{
    m_target = target;
}

//---------------------------------------------------------------------------------------------------------------------
void OrbitCamera::setDistance(float distance)
{
    m_distance = qMax(distance, minDistance);
}

//---------------------------------------------------------------------------------------------------------------------
void OrbitCamera::setAngles(float yaw, float pitch)
{
    m_yaw = std::fmod(yaw, 360.0f);
    m_pitch = qBound(minPitch, pitch, maxPitch);
}

//---------------------------------------------------------------------------------------------------------------------
QVector3D OrbitCamera::eye() const
{
    const float yawRad = qDegreesToRadians(m_yaw);
    const float pitchRad = qDegreesToRadians(m_pitch);
    const QVector3D direction(qCos(pitchRad) * qSin(yawRad), qSin(pitchRad), qCos(pitchRad) * qCos(yawRad));
    return m_target + direction * m_distance;
}

//---------------------------------------------------------------------------------------------------------------------
QMatrix4x4 OrbitCamera::viewMatrix() const
{
    QMatrix4x4 view;
    view.lookAt(eye(), m_target, QVector3D(0, 1, 0));
    return view;
}

//---------------------------------------------------------------------------------------------------------------------
QMatrix4x4 OrbitCamera::projectionMatrix(float aspectRatio) const
{
    QMatrix4x4 projection;
    const float nearPlane = qMax(m_distance * 0.01f, minDistance);
    const float farPlane = m_distance * 100.0f;
    projection.perspective(m_fieldOfView, aspectRatio > 0.0f ? aspectRatio : 1.0f, nearPlane, farPlane);
    return projection;
}

} // namespace Seamly3D
