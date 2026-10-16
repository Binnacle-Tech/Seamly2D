/******************************************************************************
 *   @file   parametricbody.cpp
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

#include "parametricbody.h"

#include <QtMath>

namespace Seamly3D
{

namespace
{

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Elliptical cross section: center + u * a * cos(t) + v * b * sin(t).
 */
struct Ring
{
    QVector3D center{};
    QVector3D u{1, 0, 0};
    QVector3D v{0, 0, 1};
    float     a{1};
    float     b{1};
};

const QVector3D axisX(1, 0, 0);
const QVector3D axisZ(0, 0, 1);

//---------------------------------------------------------------------------------------------------------------------
double sampledPerimeter(double a, double b, int segments)
{
    double perimeter = 0;
    QPointF previous(a, 0);
    for (int j = 1; j <= segments; ++j)
    {
        const double t = 2 * M_PI * j / segments;
        const QPointF point(a * qCos(t), b * qSin(t));
        const QPointF d = point - previous;
        perimeter += qSqrt(d.x() * d.x() + d.y() * d.y());
        previous = point;
    }
    return perimeter;
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief girthRing makes a ring whose sampled polygon has exactly the perimeter @p girth.
 * @param ratio width (along u) divided by depth (along v).
 */
Ring girthRing(const QVector3D &center, double girth, double ratio, int segments, const QVector3D &u = axisX,
               const QVector3D &v = axisZ)
{
    const double unit = sampledPerimeter(1.0, 1.0 / ratio, segments);
    const double scale = girth / unit;

    Ring ring;
    ring.center = center;
    ring.u = u;
    ring.v = v;
    ring.a = static_cast<float>(scale);
    ring.b = static_cast<float>(scale / ratio);
    return ring;
}

//---------------------------------------------------------------------------------------------------------------------
Ring sizeRing(const QVector3D &center, float a, float b, const QVector3D &u = axisX, const QVector3D &v = axisZ)
{
    Ring ring;
    ring.center = center;
    ring.u = u;
    ring.v = v;
    ring.a = a;
    ring.b = b;
    return ring;
}

//---------------------------------------------------------------------------------------------------------------------
float signedVolume(const Mesh &mesh)
{
    float volume = 0;
    for (int i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        const QVector3D &a = mesh.positions.at(static_cast<int>(mesh.indices.at(i)));
        const QVector3D &b = mesh.positions.at(static_cast<int>(mesh.indices.at(i + 1)));
        const QVector3D &c = mesh.positions.at(static_cast<int>(mesh.indices.at(i + 2)));
        volume += QVector3D::dotProduct(a, QVector3D::crossProduct(b, c));
    }
    return volume / 6.0f;
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief loft builds a closed tube through @p rings, capped by a point at each end, with outward facing triangles.
 */
Mesh loft(const QVector<Ring> &rings, const QVector3D &startPole, const QVector3D &endPole, int segments)
{
    Mesh mesh;
    const auto n = static_cast<quint32>(segments);
    const auto ringCount = static_cast<quint32>(rings.size());

    mesh.positions.reserve(rings.size() * segments + 2);
    for (const Ring &ring : rings)
    {
        for (int j = 0; j < segments; ++j)
        {
            const float t = static_cast<float>(2 * M_PI * j / segments);
            mesh.positions.append(ring.center + ring.u * (ring.a * qCos(t)) + ring.v * (ring.b * qSin(t)));
        }
    }
    const quint32 start = ringCount * n;
    const quint32 end = start + 1;
    mesh.positions << startPole << endPole;

    for (quint32 i = 0; i + 1 < ringCount; ++i)
    {
        for (quint32 j = 0; j < n; ++j)
        {
            const quint32 a = i * n + j;
            const quint32 b = (i + 1) * n + j;
            const quint32 c = i * n + (j + 1) % n;
            const quint32 d = (i + 1) * n + (j + 1) % n;
            mesh.indices << a << b << c << b << d << c;
        }
    }

    const quint32 last = (ringCount - 1) * n;
    for (quint32 j = 0; j < n; ++j)
    {
        // Caps run their ring edges opposite to the neighbouring strip triangles so the surface is closed.
        mesh.indices << start << j << (j + 1) % n;
        mesh.indices << end << last + (j + 1) % n << last + j;
    }

    // Ring direction and loft direction differ between parts; fix the winding once instead of reasoning per part.
    if (signedVolume(mesh) < 0)
    {
        for (int i = 0; i + 2 < mesh.indices.size(); i += 3)
        {
            std::swap(mesh.indices[i + 1], mesh.indices[i + 2]);
        }
    }

    mesh.computeNormals();
    return mesh;
}

//---------------------------------------------------------------------------------------------------------------------
float lerp(float from, float to, float t)
{
    return from + (to - from) * t;
}

} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
Mesh BodyModel::combined() const
{
    Mesh mesh = torso;
    mesh.append(leftLeg);
    mesh.append(rightLeg);
    mesh.append(leftArm);
    mesh.append(rightArm);
    return mesh;
}

//---------------------------------------------------------------------------------------------------------------------
float ParametricBody::underbustHeight(const BodyMeasurements &measurements)
{
    const auto bust = static_cast<float>(measurements.value(BodyMeasurement::BustpointHeight));
    const auto waist = static_cast<float>(measurements.value(BodyMeasurement::WaistHeight));
    return lerp(bust, waist, 0.45f);
}

//---------------------------------------------------------------------------------------------------------------------
float ParametricBody::thighHeight(const BodyMeasurements &measurements)
{
    const auto crotch = static_cast<float>(measurements.value(BodyMeasurement::CrotchHeight));
    const auto knee = static_cast<float>(measurements.value(BodyMeasurement::KneeHeight));
    return lerp(crotch, knee, 0.06f);
}

//---------------------------------------------------------------------------------------------------------------------
BodyModel ParametricBody::generate(const BodyMeasurements &measurements, int segments)
{
    segments = qMax(segments, 8);

    BodyModel model;
    model.measurements = measurements.sanitized();
    const BodyMeasurements &m = model.measurements;
    const auto value = [&m](BodyMeasurement measurement) { return static_cast<float>(m.value(measurement)); };

    const float height = value(BodyMeasurement::Height);
    const float neckBackH = value(BodyMeasurement::NeckBackHeight);
    const float shoulderH = value(BodyMeasurement::ShoulderTipHeight);
    const float armpitH = value(BodyMeasurement::ArmpitHeight);
    const float bustH = value(BodyMeasurement::BustpointHeight);
    const float waistH = value(BodyMeasurement::WaistHeight);
    const float hipH = value(BodyMeasurement::HipHeight);
    const float crotchH = value(BodyMeasurement::CrotchHeight);
    const float kneeH = value(BodyMeasurement::KneeHeight);
    const float calfH = value(BodyMeasurement::CalfHeight);
    const float ankleH = value(BodyMeasurement::AnkleHeight);

    const float hipCirc = value(BodyMeasurement::HipCirc);
    const float bustCirc = value(BodyMeasurement::BustCirc);
    const float neckCirc = value(BodyMeasurement::NeckCirc);
    const float headCirc = value(BodyMeasurement::HeadCirc);

    // ---- Torso, neck and head: one loft from the crotch to the top of the head. -----------------------------------
    QVector<Ring> torso;
    torso << girthRing(QVector3D(0, lerp(crotchH, hipH, 0.25f), -0.5f), hipCirc * 0.94f, 1.45, segments);
    const Ring hip = girthRing(QVector3D(0, hipH, -1.0f), hipCirc, 1.35, segments);
    torso << hip;
    torso << girthRing(QVector3D(0, waistH, 0), value(BodyMeasurement::WaistCirc), 1.35, segments);
    torso << girthRing(QVector3D(0, underbustHeight(m), 0.5f), value(BodyMeasurement::UnderbustCirc), 1.3, segments);
    const Ring bust = girthRing(QVector3D(0, bustH, 1.5f), bustCirc, 1.25, segments);
    torso << bust;
    const Ring armpit = girthRing(QVector3D(0, armpitH, 0.5f), bustCirc * 0.96f, 1.45, segments);
    torso << armpit;
    const float shoulderWidth = value(BodyMeasurement::ShoulderWidth);
    torso << sizeRing(QVector3D(0, shoulderH, 0), qMax(shoulderWidth * 0.46f, armpit.a * 0.9f), armpit.b * 0.8f);
    torso << girthRing(QVector3D(0, neckBackH, 0.5f), neckCirc * 1.05f, 1.1, segments);

    const float neckTopH = lerp(neckBackH, height, 0.2f);
    const float headLength = height - neckTopH;
    torso << girthRing(QVector3D(0, neckTopH, 1.0f), neckCirc, 1.0, segments);
    torso << girthRing(QVector3D(0, neckTopH + headLength * 0.12f, 1.5f), headCirc * 0.78f, 0.85, segments);
    torso << girthRing(QVector3D(0, height - headLength * 0.45f, 0.5f), headCirc, 0.82, segments);
    torso << girthRing(QVector3D(0, height - headLength * 0.2f, 0), headCirc * 0.85f, 0.85, segments);
    torso << girthRing(QVector3D(0, height - headLength * 0.06f, 0), headCirc * 0.5f, 0.9, segments);

    model.torso = loft(torso, QVector3D(0, crotchH, -0.5f), QVector3D(0, height, 0), segments);

    // ---- Legs: hanging straight down, thighs meeting in the middle like on a real body. ----------------------------
    const float thighCirc = value(BodyMeasurement::ThighCirc);
    const Ring thighShape = girthRing(QVector3D(), thighCirc, 1.1, segments);
    const float legX = qMax(hip.a - thighShape.a, thighShape.a * 0.85f);
    const float footLength = height * 0.15f;
    // The top of the leg hides inside the lower torso so the hip line stays smooth.
    const float topH = lerp(crotchH, hipH, 0.35f);

    for (const float side : {1.0f, -1.0f})
    {
        const float x = side * legX;
        QVector<Ring> leg;
        leg << girthRing(QVector3D(x * 0.9f, topH, 0), thighCirc * 0.95f, 1.15, segments);
        leg << girthRing(QVector3D(x, thighHeight(m), 0), thighCirc, 1.1, segments);
        leg << girthRing(QVector3D(x * 0.9f, lerp(crotchH, kneeH, 0.5f), 0),
                         lerp(thighCirc, value(BodyMeasurement::KneeCirc), 0.45f), 1.05, segments);
        leg << girthRing(QVector3D(x * 0.78f, kneeH, 0), value(BodyMeasurement::KneeCirc), 1.0, segments);
        leg << girthRing(QVector3D(x * 0.75f, calfH, -0.5f), value(BodyMeasurement::CalfCirc), 1.0, segments);
        const Ring ankle = girthRing(QVector3D(x * 0.7f, ankleH, 0), value(BodyMeasurement::AnkleCirc), 0.95, segments);
        leg << ankle;
        const QVector3D footCenter(x * 0.72f, ankleH * 0.4f, footLength * 0.25f);
        leg << sizeRing(footCenter, ankle.a * 1.15f, footLength * 0.5f);

        Mesh mesh = loft(leg, QVector3D(x * 0.9f, topH + 2.0f, 0), QVector3D(footCenter.x(), 0, footCenter.z()),
                         segments);
        (side > 0 ? model.leftLeg : model.rightLeg) = mesh;
    }

    // ---- Arms: A-pose, angled out just enough to clear the bust. ---------------------------------------------------
    const float armLength = value(BodyMeasurement::ArmLength);
    const float handLength = height * 0.108f;
    const Ring upperArmShape = girthRing(QVector3D(), value(BodyMeasurement::UpperArmCirc), 1.0, segments);
    const float shoulderX = qMax(shoulderWidth * 0.5f - upperArmShape.a * 0.6f, upperArmShape.a);
    const float shoulderY = shoulderH - upperArmShape.a * 0.9f;
    const float drop = qMax(shoulderY - bustH, 1.0f);
    const float needed = bust.a + upperArmShape.a * 1.05f - shoulderX;
    const float angle = qBound(qDegreesToRadians(6.0f), qAtan2(qMax(needed, 0.0f), drop) + qDegreesToRadians(2.0f),
                               qDegreesToRadians(40.0f));

    for (const float side : {1.0f, -1.0f})
    {
        const QVector3D origin(side * shoulderX, shoulderY, 0);
        const QVector3D direction(side * qSin(angle), -qCos(angle), 0);
        const QVector3D u = QVector3D::crossProduct(direction, axisZ).normalized();
        const auto at = [&](float distance) { return origin + direction * distance; };
        if (side > 0)
        {
            model.leftShoulder = origin;
            model.leftWrist = at(armLength);
        }

        QVector<Ring> arm;
        arm << girthRing(at(0), value(BodyMeasurement::UpperArmCirc) * 1.1f, 1.0, segments, u, axisZ);
        arm << girthRing(at(armLength * 0.18f), value(BodyMeasurement::UpperArmCirc), 1.0, segments, u, axisZ);
        arm << girthRing(at(armLength * 0.56f), value(BodyMeasurement::ElbowCirc), 1.0, segments, u, axisZ);
        const Ring wrist = girthRing(at(armLength), value(BodyMeasurement::WristCirc), 0.75, segments, u, axisZ);
        arm << wrist;
        arm << sizeRing(at(armLength + handLength * 0.45f), wrist.a * 0.6f, wrist.b * 1.35f, u, axisZ);
        arm << sizeRing(at(armLength + handLength * 0.85f), wrist.a * 0.5f, wrist.b * 1.0f, u, axisZ);

        Mesh mesh = loft(arm, at(-upperArmShape.a * 0.5f), at(armLength + handLength), segments);
        (side > 0 ? model.leftArm : model.rightArm) = mesh;
    }

    return model;
}

} // namespace Seamly3D
