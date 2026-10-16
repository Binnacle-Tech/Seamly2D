/******************************************************************************
 *   @file   bodymeasurements.cpp
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

#include "bodymeasurements.h"

#include <QDate>
#include <QFile>
#include <QHash>
#include <QIODevice>
#include <QXmlStreamReader>

namespace Seamly3D
{

namespace
{

struct MeasurementInfo
{
    BodyMeasurement      id;
    const char          *name;
    const char          *displayName;
    double               defaultValue; // centimeters, see below
    BodyMeasurementGroup group;
};

// Order must match the BodyMeasurement enum. The defaults are MakeHuman's average adult woman (25 years, average
// weight and muscle) measured with MakeHumanTape and scaled to 168 cm, so the default body needs no reshaping and
// measurements estimated from a height follow real proportions.
const MeasurementInfo measurementInfo[] = {
    {BodyMeasurement::Height, "height", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Height"), 168.0,
     BodyMeasurementGroup::Height},
    {BodyMeasurement::NeckBackHeight, "height_neck_back",
     QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Height: Neck Back"), 140.5, BodyMeasurementGroup::Height},
    {BodyMeasurement::ShoulderTipHeight, "height_shoulder_tip",
     QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Height: Shoulder Tip"), 137.0, BodyMeasurementGroup::Height},
    {BodyMeasurement::ArmpitHeight, "height_armpit", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Height: Armpit"),
     126.0, BodyMeasurementGroup::Height},
    {BodyMeasurement::BustpointHeight, "height_bustpoint",
     QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Height: Bustpoint"), 122.5, BodyMeasurementGroup::Height},
    {BodyMeasurement::WaistHeight, "height_waist_side",
     QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Height: Waist Side"), 103.5, BodyMeasurementGroup::Height},
    {BodyMeasurement::HipHeight, "height_hip", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Height: Hip"), 85.5,
     BodyMeasurementGroup::Height},
    {BodyMeasurement::CrotchHeight, "leg_crotch_to_floor",
     QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Leg: Crotch to floor"), 78.0, BodyMeasurementGroup::Height},
    {BodyMeasurement::KneeHeight, "height_knee", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Height: Knee"), 48.5,
     BodyMeasurementGroup::Height},
    {BodyMeasurement::CalfHeight, "height_calf", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Height: Calf"), 34.5,
     BodyMeasurementGroup::Height},
    {BodyMeasurement::AnkleHeight, "height_ankle", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Height: Ankle"),
     12.0, BodyMeasurementGroup::Height},

    {BodyMeasurement::HeadCirc, "head_circ", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Head circumference"),
     59.0, BodyMeasurementGroup::Girth},
    {BodyMeasurement::NeckCirc, "neck_circ", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Neck circumference"),
     35.5, BodyMeasurementGroup::Girth},
    {BodyMeasurement::BustCirc, "bust_circ", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Bust circumference"),
     87.0, BodyMeasurementGroup::Girth},
    {BodyMeasurement::UnderbustCirc, "lowbust_circ",
     QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Lowbust circumference"), 69.5, BodyMeasurementGroup::Girth},
    {BodyMeasurement::WaistCirc, "waist_circ", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Waist circumference"),
     75.0, BodyMeasurementGroup::Girth},
    {BodyMeasurement::HipCirc, "hip_circ", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Hip circumference"), 99.5,
     BodyMeasurementGroup::Girth},
    {BodyMeasurement::ThighCirc, "leg_thigh_upper_circ",
     QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Leg: Thigh Upper circumference"), 54.0,
     BodyMeasurementGroup::Girth},
    {BodyMeasurement::KneeCirc, "leg_knee_circ", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Leg: Knee circumference"),
     36.0, BodyMeasurementGroup::Girth},
    {BodyMeasurement::CalfCirc, "leg_calf_circ", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Leg: Calf circumference"),
     37.5, BodyMeasurementGroup::Girth},
    {BodyMeasurement::AnkleCirc, "leg_ankle_circ",
     QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Leg: Ankle circumference"), 18.5, BodyMeasurementGroup::Girth},

    {BodyMeasurement::ShoulderWidth, "shoulder_tip_to_shoulder_tip_b",
     QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Shoulder Tip to Shoulder Tip, back"), 37.5,
     BodyMeasurementGroup::Arm},
    {BodyMeasurement::ArmLength, "arm_shoulder_tip_to_wrist",
     QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Arm: Shoulder Tip to Wrist"), 49.0, BodyMeasurementGroup::Arm},
    {BodyMeasurement::UpperArmCirc, "arm_upper_circ",
     QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Arm: Upper Arm circumference"), 28.5, BodyMeasurementGroup::Arm},
    {BodyMeasurement::ElbowCirc, "arm_elbow_circ",
     QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Arm: Elbow circumference"), 24.5, BodyMeasurementGroup::Arm},
    {BodyMeasurement::WristCirc, "arm_wrist_circ",
     QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Arm: Wrist circumference"), 13.0, BodyMeasurementGroup::Arm},
};

static_assert(sizeof(measurementInfo) / sizeof(measurementInfo[0]) == static_cast<size_t>(BodyMeasurement::Count),
              "measurementInfo must describe every BodyMeasurement");

// Male defaults in BodyMeasurement order: MakeHuman's average adult man (25 years, average weight and muscle)
// measured with MakeHumanTape and scaled to 178 cm, rounded to 0.5 cm.
const double maleDefaults[] = {
    178.0, 149.5, 146.5, 134.0, 130.0, 108.0, 93.0, 85.5, 55.0, 39.5, 11.0, // heights
    61.0,  46.0,  99.0,  86.5,  79.0,  96.0,  56.0, 37.5, 39.5, 23.5,       // girths
    40.0,  56.5,  34.0,  28.5,  15.5};                                     // shoulders and arms

static_assert(sizeof(maleDefaults) / sizeof(maleDefaults[0]) == static_cast<size_t>(BodyMeasurement::Count),
              "maleDefaults must have a value for every BodyMeasurement");

// How to take each measurement, in BodyMeasurement order: number in the measurement database, the diagram of
// the Seamly2D diagram resource (see MapDiagrams() in vpatterndb) and the database's description.
struct MeasurementGuide
{
    const char *code;
    const char *diagram;
    const char *description;
};

const MeasurementGuide guides[] = {
    {"A01", "Ap1", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Vertical distance from crown of head to floor.")},
    {"A02", "Ap1", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements",
                                     "Vertical distance from the Neck Back (cervicale vertebra) to the floor.")},
    {"A15", "Ap1", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements",
                                     "Vertical distance from the Shoulder Tip to the floor.")},
    {"A04", "Ap1", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Vertical distance from the Armpit to the floor.")},
    {"A14", "Ap1", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements",
                                     "Vertical distance from Bustpoint to the floor.")},
    {"A05", "Ap1", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements",
                                     "Vertical distance from the Waist Side to the floor.")},
    {"A06", "Ap1", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements",
                                     "Vertical distance from the Hip level to the floor.")},
    {"M01", "Mp1", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Stand feet close together. Measure from crotch "
                                     "level (touching body, no extra space) down to floor.")},
    {"A08", "Ap1", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements",
                                     "Vertical distance from the fold at the back of the Knee to the floor.")},
    {"A09", "Ap1", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements",
                                     "Vertical distance from the widest point of the calf to the floor.")},
    {"A11", "Ap1", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements",
                                     "Vertical distance from point where the front leg meets the foot to the floor.")},

    {"F01", "Fp1", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Measure circumference at largest level of head.")},
    {"G02", "Gp1", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Neck circumference at base of Neck, touching "
                                     "Neck Back, Neck Sides, and Neck Front.")},
    {"G04", "Gp1", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Circumference around Bust, parallel to floor.")},
    {"G05", "Gp1", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements",
                                     "Circumference around LowBust under the breasts, parallel to floor.")},
    {"G07", "Gp1", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Circumference around Waist, following natural "
                                     "contours. Waists are typically higher in back.")},
    {"G09", "Gp1", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Circumference around Hip where Hip protrusion is "
                                     "greatest, parallel to floor.")},
    {"M03", "Mp2", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements",
                                     "Thigh circumference at the fullest part of the upper Thigh near the Crotch.")},
    {"M05", "Mp2", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Knee circumference with straight leg.")},
    {"M07", "Mp2", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements",
                                     "Calf circumference at the largest part of lower leg.")},
    {"M09", "Mp2", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements",
                                     "Ankle circumference where front of leg meets the top of the foot.")},

    {"I07", "Ip3", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "From Shoulder Tip to Shoulder Tip, across the back.")},
    {"L05", "Lp2", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "From Shoulder Tip to Wrist bone, arm straight.")},
    {"L11", "Lp4", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Arm circumference at Armpit level.")},
    {"L13", "Lp4", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Elbow circumference, arm straight.")},
    {"L15", "Lp4", QT_TRANSLATE_NOOP("Seamly3D::BodyMeasurements", "Wrist circumference.")},
};

static_assert(sizeof(guides) / sizeof(guides[0]) == static_cast<size_t>(BodyMeasurement::Count),
              "guides must describe every BodyMeasurement");

const BodyMeasurement verticalOrder[] = {
    BodyMeasurement::Height,       BodyMeasurement::NeckBackHeight,  BodyMeasurement::ShoulderTipHeight,
    BodyMeasurement::ArmpitHeight, BodyMeasurement::BustpointHeight, BodyMeasurement::WaistHeight,
    BodyMeasurement::HipHeight,    BodyMeasurement::CrotchHeight,    BodyMeasurement::KneeHeight,
    BodyMeasurement::CalfHeight,   BodyMeasurement::AnkleHeight};

const MeasurementInfo &info(BodyMeasurement measurement)
{
    return measurementInfo[static_cast<int>(measurement)];
}

//---------------------------------------------------------------------------------------------------------------------
double unitToCentimeters(const QString &unit)
{
    if (unit == QLatin1String("mm"))
    {
        return 0.1;
    }
    if (unit == QLatin1String("inch"))
    {
        return 2.54;
    }
    return 1.0;
}

} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
BodyMeasurements::BodyMeasurements()
    : m_values(count())
{
    for (int i = 0; i < count(); ++i)
    {
        m_values[i] = measurementInfo[i].defaultValue;
    }
}

//---------------------------------------------------------------------------------------------------------------------
double BodyMeasurements::value(BodyMeasurement measurement) const
{
    return m_values.at(static_cast<int>(measurement));
}

//---------------------------------------------------------------------------------------------------------------------
void BodyMeasurements::setValue(BodyMeasurement measurement, double centimeters)
{
    m_values[static_cast<int>(measurement)] = centimeters;
}

//---------------------------------------------------------------------------------------------------------------------
double BodyMeasurements::gender() const
{
    return m_gender;
}

//---------------------------------------------------------------------------------------------------------------------
void BodyMeasurements::setGender(double gender)
{
    m_gender = qBound(0.0, gender, 1.0);
}

//---------------------------------------------------------------------------------------------------------------------
double BodyMeasurements::muscle() const
{
    return m_muscle;
}

//---------------------------------------------------------------------------------------------------------------------
void BodyMeasurements::setMuscle(double muscle)
{
    m_muscle = qBound(0.0, muscle, 1.0);
}

//---------------------------------------------------------------------------------------------------------------------
double BodyMeasurements::age() const
{
    return m_age;
}

//---------------------------------------------------------------------------------------------------------------------
void BodyMeasurements::setAge(double years)
{
    m_age = qBound(1.0, years, 90.0);
}

//---------------------------------------------------------------------------------------------------------------------
void BodyMeasurements::scaleToHeight(double height)
{
    const double current = value(BodyMeasurement::Height);
    if (height <= 0 || current <= 0)
    {
        return;
    }

    // Everything scales, girths too: scaling only the lengths would make a stretched, thin body.
    const double factor = height / current;
    for (double &measurement : m_values)
    {
        measurement *= factor;
    }
    setValue(BodyMeasurement::Height, height); // avoid rounding drift
}

//---------------------------------------------------------------------------------------------------------------------
BodyMeasurements BodyMeasurements::sanitized() const
{
    BodyMeasurements result = *this;
    for (int i = 0; i < count(); ++i)
    {
        if (!(result.m_values.at(i) > 0.0)) // also catches NaN
        {
            result.m_values[i] = defaultValue(static_cast<BodyMeasurement>(i), m_gender);
        }
    }

    // Heights must strictly decrease from the top of the head to the ankle.
    const double height = result.value(BodyMeasurement::Height);
    const double minGap = height * 0.005;
    double previous = height;
    for (const BodyMeasurement measurement : verticalOrder)
    {
        if (measurement == BodyMeasurement::Height)
        {
            continue;
        }
        const double limit = previous - minGap;
        if (result.value(measurement) > limit)
        {
            result.setValue(measurement, limit);
        }
        previous = result.value(measurement);
    }
    if (previous <= 0)
    {
        // Measurements were so inconsistent that the ankle went under the floor: fall back to proportions.
        const BodyMeasurements proportional = [height]()
        {
            BodyMeasurements defaults;
            defaults.scaleToHeight(height);
            return defaults;
        }();
        for (const BodyMeasurement measurement : verticalOrder)
        {
            result.setValue(measurement, proportional.value(measurement));
        }
    }

    return result;
}

//---------------------------------------------------------------------------------------------------------------------
bool BodyMeasurements::operator==(const BodyMeasurements &other) const
{
    return m_values == other.m_values && qFuzzyCompare(m_gender + 1, other.m_gender + 1)
           && qFuzzyCompare(m_muscle + 1, other.m_muscle + 1) && qFuzzyCompare(m_age, other.m_age);
}

//---------------------------------------------------------------------------------------------------------------------
bool BodyMeasurements::operator!=(const BodyMeasurements &other) const
{
    return !(*this == other);
}

//---------------------------------------------------------------------------------------------------------------------
bool BodyMeasurements::readVitFile(const QString &fileName, QStringList *applied)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        m_errorString = tr("Cannot open file %1: %2").arg(fileName, file.errorString());
        return false;
    }
    return readVit(&file, applied);
}

//---------------------------------------------------------------------------------------------------------------------
bool BodyMeasurements::readVit(QIODevice *device, QStringList *applied)
{
    m_errorString.clear();

    QHash<QString, BodyMeasurement> byName;
    for (int i = 0; i < count(); ++i)
    {
        byName.insert(name(static_cast<BodyMeasurement>(i)), static_cast<BodyMeasurement>(i));
    }

    QString unit = QStringLiteral("cm");
    QString gender;
    QDate birthDate;
    QHash<BodyMeasurement, double> found;
    bool isIndividualFile = false;

    QXmlStreamReader xml(device);
    while (!xml.atEnd())
    {
        if (xml.readNext() != QXmlStreamReader::StartElement)
        {
            continue;
        }

        if (xml.name() == QLatin1String("smis"))
        {
            isIndividualFile = true;
        }
        else if (xml.name() == QLatin1String("unit"))
        {
            unit = xml.readElementText().trimmed();
        }
        else if (xml.name() == QLatin1String("gender"))
        {
            gender = xml.readElementText().trimmed();
        }
        else if (xml.name() == QLatin1String("birth-date"))
        {
            birthDate = QDate::fromString(xml.readElementText().trimmed(), QStringLiteral("yyyy-MM-dd"));
        }
        else if (xml.name() == QLatin1String("m"))
        {
            const QXmlStreamAttributes attributes = xml.attributes();
            const auto it = byName.constFind(attributes.value(QLatin1String("name")).toString());
            if (it == byName.constEnd())
            {
                continue;
            }

            // Values are stored with '.' as decimal separator. Formulas are not evaluated here.
            bool ok = false;
            const double number = attributes.value(QLatin1String("value")).toString().trimmed().toDouble(&ok);
            if (ok && number > 0)
            {
                found.insert(it.value(), number);
            }
        }
    }

    if (xml.hasError())
    {
        m_errorString = tr("Invalid measurements file: %1 (line %2)").arg(xml.errorString()).arg(xml.lineNumber());
        return false;
    }

    if (!isIndividualFile)
    {
        m_errorString = tr("Not an individual measurements (.vit) file.");
        return false;
    }

    const double toCentimeters = unitToCentimeters(unit);

    // Start from a default body of the right height, then apply what the file has.
    BodyMeasurements result;
    if (found.contains(BodyMeasurement::Height))
    {
        result.scaleToHeight(found.value(BodyMeasurement::Height) * toCentimeters);
    }

    QStringList names;
    for (auto it = found.constBegin(); it != found.constEnd(); ++it)
    {
        result.setValue(it.key(), it.value() * toCentimeters);
        names.append(name(it.key()));
    }
    names.sort();

    if (gender == QLatin1String("male"))
    {
        result.setGender(1.0);
    }
    else if (gender == QLatin1String("female"))
    {
        result.setGender(0.0);
    }
    else
    {
        result.setGender(m_gender);
    }
    result.setMuscle(m_muscle);

    // SeamlyMe writes 1800-01-01 when the birth date is unknown; only use plausible ages.
    result.setAge(m_age);
    if (birthDate.isValid())
    {
        const double years = birthDate.daysTo(QDate::currentDate()) / 365.25;
        if (years >= 1.0 && years <= 90.0)
        {
            result.setAge(years);
        }
    }

    m_values = result.m_values;
    m_gender = result.m_gender;
    m_muscle = result.m_muscle;
    m_age = result.m_age;
    if (applied != nullptr)
    {
        *applied = names;
    }
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
QString BodyMeasurements::errorString() const
{
    return m_errorString;
}

//---------------------------------------------------------------------------------------------------------------------
int BodyMeasurements::count()
{
    return static_cast<int>(BodyMeasurement::Count);
}

//---------------------------------------------------------------------------------------------------------------------
QString BodyMeasurements::name(BodyMeasurement measurement)
{
    return QString::fromLatin1(info(measurement).name);
}

//---------------------------------------------------------------------------------------------------------------------
QString BodyMeasurements::displayName(BodyMeasurement measurement)
{
    return tr(info(measurement).displayName);
}

//---------------------------------------------------------------------------------------------------------------------
QString BodyMeasurements::code(BodyMeasurement measurement)
{
    return QString::fromLatin1(guides[static_cast<int>(measurement)].code);
}

//---------------------------------------------------------------------------------------------------------------------
QString BodyMeasurements::description(BodyMeasurement measurement)
{
    return tr(guides[static_cast<int>(measurement)].description);
}

//---------------------------------------------------------------------------------------------------------------------
QString BodyMeasurements::diagram(BodyMeasurement measurement)
{
    return QStringLiteral(":/diagrams/%1.svg").arg(QString::fromLatin1(guides[static_cast<int>(measurement)].diagram));
}

//---------------------------------------------------------------------------------------------------------------------
double BodyMeasurements::defaultValue(BodyMeasurement measurement)
{
    return info(measurement).defaultValue;
}

//---------------------------------------------------------------------------------------------------------------------
double BodyMeasurements::defaultValue(BodyMeasurement measurement, double gender)
{
    const double g = qBound(0.0, gender, 1.0);
    return (1 - g) * info(measurement).defaultValue + g * maleDefaults[static_cast<int>(measurement)];
}

//---------------------------------------------------------------------------------------------------------------------
BodyMeasurements BodyMeasurements::defaults(double gender)
{
    BodyMeasurements result;
    result.setGender(gender);
    for (int i = 0; i < count(); ++i)
    {
        result.m_values[i] = defaultValue(static_cast<BodyMeasurement>(i), result.m_gender);
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
bool BodyMeasurements::hasDefaultValues() const
{
    for (int i = 0; i < count(); ++i)
    {
        if (!qFuzzyCompare(m_values.at(i), defaultValue(static_cast<BodyMeasurement>(i), m_gender)))
        {
            return false;
        }
    }
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
BodyMeasurementGroup BodyMeasurements::group(BodyMeasurement measurement)
{
    return info(measurement).group;
}


} // namespace Seamly3D
