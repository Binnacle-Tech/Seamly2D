/******************************************************************************
 *   @file   bodymeasurements.h
 **  @author Seamly2D project
 **  @date   15 10, 2026
 **
 **  @brief  Body measurements that drive the parametric body.
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

#ifndef SEAMLY3D_BODYMEASUREMENTS_H
#define SEAMLY3D_BODYMEASUREMENTS_H

#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QVector>

class QIODevice;

namespace Seamly3D
{

/**
 * @brief Body measurements that drive the parametric body. Names match the Seamly2D/SeamlyMe measurement database.
 */
enum class BodyMeasurement : int
{
    Height = 0,           // height                         A01
    NeckBackHeight,       // height_neck_back               A02
    ShoulderTipHeight,    // height_shoulder_tip            A15
    ArmpitHeight,         // height_armpit                  A04
    BustpointHeight,      // height_bustpoint               A14
    WaistHeight,          // height_waist_side              A05
    HipHeight,            // height_hip                     A06
    CrotchHeight,         // leg_crotch_to_floor            M01
    KneeHeight,           // height_knee                    A08
    CalfHeight,           // height_calf                    A09
    AnkleHeight,          // height_ankle                   A11

    HeadCirc,             // head_circ                      F01
    NeckCirc,             // neck_circ                      G02
    BustCirc,             // bust_circ                      G04
    UnderbustCirc,        // lowbust_circ                   G05
    WaistCirc,            // waist_circ                     G07
    HipCirc,              // hip_circ                       G09
    ThighCirc,            // leg_thigh_upper_circ           M03
    KneeCirc,             // leg_knee_circ                  M05
    CalfCirc,             // leg_calf_circ                  M07
    AnkleCirc,            // leg_ankle_circ                 M09

    ShoulderWidth,        // shoulder_tip_to_shoulder_tip_b I07
    ArmLength,            // arm_shoulder_tip_to_wrist      L05
    UpperArmCirc,         // arm_upper_circ                 L11
    ElbowCirc,            // arm_elbow_circ                 L13
    WristCirc,            // arm_wrist_circ                 L15

    Count
};

enum class BodyMeasurementGroup : char { Height, Girth, Arm };

/**
 * @brief A complete set of body measurements in centimeters.
 *
 * Every measurement always has a value: missing ones come from a default body, scaled to the given height.
 */
class BodyMeasurements
{
    Q_DECLARE_TR_FUNCTIONS(Seamly3D::BodyMeasurements)

public:
    BodyMeasurements();

    double  value(BodyMeasurement measurement) const;
    void    setValue(BodyMeasurement measurement, double centimeters);

    /** @brief Body build that measurements do not capture: gender 0 (female) to 1 (male), muscle 0 to 1. */
    double  gender() const;
    void    setGender(double gender);
    double  muscle() const;
    void    setMuscle(double muscle);
    double  age() const;               ///< years, 1 to 90
    void    setAge(double years);

    /** @brief scaleToHeight changes the height and scales every vertical measurement by the same factor. */
    void    scaleToHeight(double height);

    /**
     * @brief sanitized returns a copy where heights strictly decrease from head to ankle and every value is positive,
     * so the generator always gets a buildable body.
     */
    BodyMeasurements sanitized() const;

    bool operator==(const BodyMeasurements &other) const;
    bool operator!=(const BodyMeasurements &other) const;

    /**
     * @brief readVit reads a SeamlyMe individual measurements file (.vit). Measurements the body does not use, or
     * whose value is a formula instead of a number, are ignored. Missing height measurements are derived from the
     * file's height. The gender is taken from the personal information when it is male or female, and the age from the
     * birth date when it is known.
     * @param applied receives the Seamly names of the measurements taken from the file.
     */
    bool    readVit(QIODevice *device, QStringList *applied = nullptr);
    bool    readVitFile(const QString &fileName, QStringList *applied = nullptr);
    QString errorString() const;

    static int     count();
    static QString name(BodyMeasurement measurement);        ///< Seamly measurement name, e.g. "bust_circ".
    static QString displayName(BodyMeasurement measurement); ///< Translated, human readable name.
    static double  defaultValue(BodyMeasurement measurement);
    static BodyMeasurementGroup group(BodyMeasurement measurement);
    static bool    isVertical(BodyMeasurement measurement);  ///< Scales with height.

private:
    QVector<double> m_values;
    double          m_gender{0.0};
    double          m_muscle{0.5};
    double          m_age{25.0};
    QString         m_errorString{};
};

} // namespace Seamly3D

#endif // SEAMLY3D_BODYMEASUREMENTS_H
