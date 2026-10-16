/******************************************************************************
 *   @file   piecearrangement.cpp
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

#include "piecearrangement.h"

#include <QtMath>

#include <functional>

namespace Seamly3D
{

namespace
{

const float clearance = 4.0f;     // centimeters between the body and the pieces wrapped around it
const float asideSpacing = 15.0f; // centimeters between pieces hung beside the body

//---------------------------------------------------------------------------------------------------------------------
/** @brief wrap bends a piece around a vertical cylinder; @p angle is where the piece's u = 0 line goes. */
Mesh wrap(const Mesh &flat, float centerX, float centerZ, float radius, float angle, float top,
          const std::function<float(const QVector3D &)> &u, const std::function<float(const QVector3D &)> &v)
{
    Mesh mesh = flat;
    for (QVector3D &p : mesh.positions)
    {
        const float phi = angle + u(p) / radius;
        const float y = top + v(p);
        p = QVector3D(centerX + radius * qSin(phi), y, centerZ + radius * qCos(phi));
    }
    mesh.computeNormals();
    return mesh;
}

//---------------------------------------------------------------------------------------------------------------------
PiecePlacement opposite(PiecePlacement placement)
{
    switch (placement)
    {
        case PiecePlacement::LeftSide:
            return PiecePlacement::RightSide;
        case PiecePlacement::RightSide:
            return PiecePlacement::LeftSide;
        case PiecePlacement::LeftSleeve:
            return PiecePlacement::RightSleeve;
        case PiecePlacement::RightSleeve:
            return PiecePlacement::LeftSleeve;
        default:
            return placement;
    }
}

} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
BodyFrame BodyFrame::fromMeasurements(const BodyMeasurements &measurements)
{
    const BodyMeasurements m = measurements.sanitized();
    const auto value = [&m](BodyMeasurement measurement) { return static_cast<float>(m.value(measurement)); };

    BodyFrame frame;
    frame.neckHeight = value(BodyMeasurement::NeckBackHeight);
    frame.shoulderHeight = value(BodyMeasurement::ShoulderTipHeight);
    const float girth = qMax(value(BodyMeasurement::BustCirc), value(BodyMeasurement::HipCirc));
    frame.torsoRadius = girth / (2 * static_cast<float>(M_PI)) + clearance;
    frame.armRadius = value(BodyMeasurement::UpperArmCirc) / (2 * static_cast<float>(M_PI));
    frame.sleeveRadius = frame.armRadius + clearance;

    const float angle = qDegreesToRadians(40.0f);
    frame.leftShoulder = QVector3D(value(BodyMeasurement::ShoulderWidth) * 0.45f, frame.shoulderHeight - frame.armRadius, 0);
    frame.leftWrist = frame.leftShoulder
                      + QVector3D(qSin(angle), -qCos(angle), 0) * value(BodyMeasurement::ArmLength);
    return frame;
}

//---------------------------------------------------------------------------------------------------------------------
BodyFrame BodyFrame::withArm(const QVector3D &shoulder, const QVector3D &wrist) const
{
    BodyFrame frame = *this;
    if ((wrist - shoulder).length() > 1.0f)
    {
        frame.leftShoulder = shoulder;
        frame.leftWrist = wrist;
    }
    return frame;
}

//---------------------------------------------------------------------------------------------------------------------
PiecePlacement PieceArrangement::guess(const QString &name)
{
    const QString n = name.toLower();
    const auto has = [&n](std::initializer_list<const char *> words)
    {
        for (const char *word : words)
        {
            if (n.contains(QString::fromUtf8(word)))
            {
                return true;
            }
        }
        return false;
    };

    // English, then the languages Seamly2D is most used in (German, French, Spanish, Dutch, Italian...).
    if (has({"sleeve", "ärmel", "aermel", "manche", "manga", "mouw", "manica"}))
    {
        return PiecePlacement::LeftSleeve;
    }
    if (has({"collar", "pocket", "cuff", "facing", "band", "belt", "yoke", "kragen", "tasche", "col", "poche",
             "cuello", "bolsillo", "kraag", "zak"}))
    {
        return PiecePlacement::Aside;
    }
    if (has({"back", "rück", "rueck", "dos", "espalda", "rug", "dietro", "retro"}))
    {
        return PiecePlacement::Back;
    }
    if (has({"side", "seite", "côté", "cote", "lado", "zij"}))
    {
        return PiecePlacement::LeftSide;
    }
    if (has({"front", "vorder", "devant", "delantero", "voor", "davanti"}))
    {
        return PiecePlacement::Front;
    }
    return PiecePlacement::Aside;
}

//---------------------------------------------------------------------------------------------------------------------
QString PieceArrangement::displayName(PiecePlacement placement)
{
    switch (placement)
    {
        case PiecePlacement::Front:
            return tr("Front");
        case PiecePlacement::Back:
            return tr("Back");
        case PiecePlacement::LeftSide:
            return tr("Left side");
        case PiecePlacement::RightSide:
            return tr("Right side");
        case PiecePlacement::LeftSleeve:
            return tr("Left sleeve");
        case PiecePlacement::RightSleeve:
            return tr("Right sleeve");
        case PiecePlacement::Aside:
            return tr("Beside the body");
        case PiecePlacement::Hidden:
        default:
            return tr("Hidden");
    }
}

//---------------------------------------------------------------------------------------------------------------------
QVector<Mesh> PieceArrangement::place(const Mesh &flat, const QRectF &bounds, PiecePlacement placement, bool pair,
                                      bool flip, const BodyFrame &frame, float asideStart)
{
    QVector<Mesh> result;
    if (flat.isEmpty() || placement == PiecePlacement::Hidden)
    {
        return result;
    }

    const auto minX = static_cast<float>(bounds.left());
    const auto maxX = static_cast<float>(bounds.right());
    const auto centerX = static_cast<float>(bounds.center().x());
    const auto topY = static_cast<float>(bounds.top());
    const auto width = static_cast<float>(bounds.width());

    // Pattern Y points down; up the body is minus Y. The top of the piece goes to the top of its place.
    const auto v = [topY](const QVector3D &p) { return topY - p.y(); };
    const float sign = flip ? -1.0f : 1.0f;
    const auto centered = [centerX, sign](const QVector3D &p) { return sign * (p.x() - centerX); };

    switch (placement)
    {
        case PiecePlacement::Front:
        case PiecePlacement::Back:
        {
            const float angle = placement == PiecePlacement::Front ? 0.0f : static_cast<float>(M_PI);
            if (!pair)
            {
                result.append(wrap(flat, 0, 0, frame.torsoRadius, angle, frame.neckHeight, centered, v));
                break;
            }
            // A half piece: one edge on the center line, the mirrored copy on the other side of it.
            const auto fromEdge = [minX, maxX, flip](const QVector3D &p) { return flip ? maxX - p.x() : p.x() - minX; };
            result.append(wrap(flat, 0, 0, frame.torsoRadius, angle, frame.neckHeight, fromEdge, v));
            result.append(wrap(flat, 0, 0, frame.torsoRadius, angle, frame.neckHeight,
                               [&fromEdge](const QVector3D &p) { return -fromEdge(p); }, v));
            break;
        }
        case PiecePlacement::LeftSide:
        case PiecePlacement::RightSide:
        {
            const auto angleOf = [](PiecePlacement side)
            { return side == PiecePlacement::LeftSide ? static_cast<float>(M_PI / 2) : static_cast<float>(-M_PI / 2); };
            result.append(wrap(flat, 0, 0, frame.torsoRadius, angleOf(placement), frame.neckHeight, centered, v));
            if (pair)
            {
                result.append(wrap(flat, 0, 0, frame.torsoRadius, angleOf(opposite(placement)), frame.neckHeight,
                                   [&centered](const QVector3D &p) { return -centered(p); }, v));
            }
            break;
        }
        case PiecePlacement::LeftSleeve:
        case PiecePlacement::RightSleeve:
        {
            // A sleeve wraps around a cylinder along the arm: the top of the cap at the shoulder, the hem toward the
            // wrist, the cap's center line on the outside of the arm.
            const auto sleeve = [&](PiecePlacement side, float mirror)
            {
                const float sideSign = side == PiecePlacement::LeftSleeve ? 1.0f : -1.0f;
                const QVector3D shoulder(sideSign * frame.leftShoulder.x(), frame.leftShoulder.y(),
                                         frame.leftShoulder.z());
                const QVector3D wrist(sideSign * frame.leftWrist.x(), frame.leftWrist.y(), frame.leftWrist.z());
                const QVector3D down = (wrist - shoulder).normalized();
                // Outward: square to the arm, toward the side the arm hangs to; forward completes the frame.
                const QVector3D side3(sideSign, 0, 0);
                QVector3D out = side3 - down * QVector3D::dotProduct(side3, down);
                out = out.isNull() ? QVector3D(0, 0, 1) : out.normalized();
                const QVector3D forward = QVector3D::crossProduct(out, down).normalized() * -sideSign;
                const QVector3D axisTop = shoulder - out * (frame.armRadius * 0.8f);

                Mesh mesh = flat;
                for (QVector3D &p : mesh.positions)
                {
                    const float phi = mirror * centered(p) / frame.sleeveRadius;
                    const QVector3D around = out * qCos(phi) + forward * qSin(phi);
                    p = axisTop - down * v(p) + around * frame.sleeveRadius;
                }
                mesh.computeNormals();
                return mesh;
            };
            result.append(sleeve(placement, 1.0f));
            if (pair)
            {
                result.append(sleeve(opposite(placement), -1.0f));
            }
            break;
        }
        case PiecePlacement::Aside:
        default:
        {
            // A row of flat pieces hung behind the body, facing it, like on a rack.
            const float z = -(frame.torsoRadius + 30.0f);
            const float step = width + asideSpacing;
            const float start = asideStart + width / 2;
            const auto flatAt = [&](float offset, float mirror)
            {
                Mesh mesh = flat;
                for (QVector3D &p : mesh.positions)
                {
                    p = QVector3D(-(start + offset) - mirror * centered(p), frame.neckHeight + v(p), z);
                }
                mesh.computeNormals();
                return mesh;
            };
            result.append(flatAt(0, 1.0f));
            if (pair)
            {
                result.append(flatAt(step, -1.0f));
            }
            break;
        }
    }
    return result;
}

} // namespace Seamly3D
