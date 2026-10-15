/******************************************************************************
 *   @file   makehumanmodel.cpp
 **  @brief  Base body, targets, macros and tape measures of MakeHuman.
 **
 **  Ported to C++ for Seamly2D from MakeHuman 1.x
 **  (https://github.com/makehumancommunity/makehuman), apps/human.py, apps/humanmodifier.py, lib/targets.py and plugins/0_modeling_a_measurement.py.
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

#include "makehumanmodel.h"

#include <QDataStream>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QtMath>

#include <cstring>
#include <limits>

// Resources of a static library must be initialized explicitly, from the global namespace.
static void initMakeHumanResource()
{
    Q_INIT_RESOURCE(makehuman);
}

namespace Seamly3D
{

namespace
{

const QString resourcePath = QStringLiteral(":/seamly3d/makehuman/makehuman_body.smh");
const char    packMagic[4] = {'S', 'M', 'H', 'B'};
const quint32 packVersion = 2;

// Names of the MacroValue ids as they appear in target names (lib/targets.py, _cat_data).
const char *const macroValueNames[] = {
    "male", "female",
    "baby", "child", "young", "old",
    "caucasian", "asian", "african",
    "minmuscle", "averagemuscle", "maxmuscle",
    "minweight", "averageweight", "maxweight",
    "minheight", "averageheight", "maxheight",
    "mincup", "averagecup", "maxcup",
    "minfirmness", "averagefirmness", "maxfirmness",
    "uncommonproportions", "regularproportions", "idealproportions"};

static_assert(sizeof(macroValueNames) / sizeof(macroValueNames[0]) == static_cast<size_t>(MacroValue::Count),
              "macroValueNames must name every MacroValue");

const QHash<QString, int> &macroValueIds()
{
    static const QHash<QString, int> ids = []()
    {
        QHash<QString, int> result;
        for (int i = 0; i < static_cast<int>(MacroValue::Count); ++i)
        {
            result.insert(QString::fromLatin1(macroValueNames[i]), i);
        }
        return result;
    }();
    return ids;
}

QVector<int> factorIds(const QString &name)
{
    QVector<int> ids;
    for (const QString &token : name.section(QLatin1Char('/'), -1).split(QLatin1Char('-'), Qt::SkipEmptyParts))
    {
        const int id = macroValueIds().value(token, -1);
        if (id >= 0)
        {
            ids.append(id);
        }
    }
    return ids;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief A modifier and the targets it moves: decrease targets for negative values, increase for positive ones. */
struct ModifierSpec
{
    QString     name;
    QStringList decrease;
    QStringList increase;
};

const QVector<ModifierSpec> &modifierSpecs()
{
    static const QVector<ModifierSpec> specs = []()
    {
        QVector<ModifierSpec> result;

        // measurement_modifiers.json: every ruler has a matching measure-*-decr|incr modifier.
        const QStringList measures = {
            QStringLiteral("neck-circ"),       QStringLiteral("neck-height"),       QStringLiteral("upperarm-circ"),
            QStringLiteral("upperarm-length"), QStringLiteral("lowerarm-length"),   QStringLiteral("wrist-circ"),
            QStringLiteral("bust-circ"),       QStringLiteral("underbust-circ"),
            QStringLiteral("waist-circ"),      QStringLiteral("napetowaist-dist"),  QStringLiteral("waisttohip-dist"),
            QStringLiteral("shoulder-dist"),   QStringLiteral("hips-circ"),         QStringLiteral("upperleg-height"),
            QStringLiteral("thigh-circ"),      QStringLiteral("lowerleg-height"),   QStringLiteral("calf-circ"),
            QStringLiteral("knee-circ"),       QStringLiteral("ankle-circ")};
        for (const QString &measure : measures)
        {
            const QString base = QStringLiteral("measure/measure-") + measure;
            result.append({QStringLiteral("measure-") + measure, {base + QStringLiteral("-decr")},
                           {base + QStringLiteral("-incr")}});
        }

        // Modeling targets for lengths and landmark heights. Left and right move together.
        const auto pair = [](const QString &name, const QString &target, const QString &minSuffix,
                             const QString &maxSuffix)
        { return ModifierSpec{name, {target + minSuffix}, {target + maxSuffix}}; };
        const QString decr = QStringLiteral("-decr");
        const QString incr = QStringLiteral("-incr");

        result.append(pair(QStringLiteral("lowerlegs-height"), QStringLiteral("armslegs/lowerlegs-height"), decr, incr));
        result.append(pair(QStringLiteral("upperlegs-height"), QStringLiteral("armslegs/upperlegs-height"), decr, incr));
        result.append({QStringLiteral("feet-scale-vert"),
                       {QStringLiteral("armslegs/l-foot-scale-vert-decr"), QStringLiteral("armslegs/r-foot-scale-vert-decr")},
                       {QStringLiteral("armslegs/l-foot-scale-vert-incr"), QStringLiteral("armslegs/r-foot-scale-vert-incr")}});
        result.append({QStringLiteral("lowerlegs-scale-vert"),
                       {QStringLiteral("armslegs/l-lowerleg-scale-vert-decr"),
                        QStringLiteral("armslegs/r-lowerleg-scale-vert-decr")},
                       {QStringLiteral("armslegs/l-lowerleg-scale-vert-incr"),
                        QStringLiteral("armslegs/r-lowerleg-scale-vert-incr")}});
        result.append(pair(QStringLiteral("hip-scale-vert"), QStringLiteral("hip/hip-scale-vert"), decr, incr));
        result.append(pair(QStringLiteral("hip-waist"), QStringLiteral("hip/hip-waist"), QStringLiteral("-down"),
                           QStringLiteral("-up")));
        result.append(pair(QStringLiteral("torso-scale-vert"), QStringLiteral("torso/torso-scale-vert"), decr, incr));
        result.append(pair(QStringLiteral("breast-trans"), QStringLiteral("breast/breast-trans"), QStringLiteral("-down"),
                           QStringLiteral("-up")));
        result.append(pair(QStringLiteral("neck-scale-vert"), QStringLiteral("neck/neck-scale-vert"), decr, incr));
        result.append({QStringLiteral("head-scale"),
                       {QStringLiteral("head/head-scale-horiz-decr"), QStringLiteral("head/head-scale-depth-decr")},
                       {QStringLiteral("head/head-scale-horiz-incr"), QStringLiteral("head/head-scale-depth-incr")}});
        result.append({QStringLiteral("upperarms-fat"),
                       {QStringLiteral("armslegs/l-upperarm-fat-decr"), QStringLiteral("armslegs/r-upperarm-fat-decr")},
                       {QStringLiteral("armslegs/l-upperarm-fat-incr"), QStringLiteral("armslegs/r-upperarm-fat-incr")}});
        result.append({QStringLiteral("lowerarms-fat"),
                       {QStringLiteral("armslegs/l-lowerarm-fat-decr"), QStringLiteral("armslegs/r-lowerarm-fat-decr")},
                       {QStringLiteral("armslegs/l-lowerarm-fat-incr"), QStringLiteral("armslegs/r-lowerarm-fat-incr")}});
        result.append(pair(QStringLiteral("shoulders-height"), QStringLiteral("seamly/shoulders"), QStringLiteral("-down"),
                           QStringLiteral("-up")));
        return result;
    }();
    return specs;
}

// Tape measure paths over the base mesh vertices (plugins/0_modeling_a_measurement.py, class Ruler).
// The length is the sum of the segments between consecutive vertices.
const QHash<QString, QVector<int>> &rulers()
{
    static const QHash<QString, QVector<int>> table = {
        {QStringLiteral("measure-neck-circ"),
         {7514, 10358, 7631, 7496, 7488, 7489, 7474, 7475, 7531, 7537, 7543, 7549, 7555, 7561,
          7743, 7722, 856,  1030, 1051, 850,  844,  838,  832,  826,  820,  756,  755,  770,
          769,  777,  929,  3690, 804,  800,  808,  801,  799,  803,  7513, 7515, 7521, 7514}},
        {QStringLiteral("measure-neck-height"), {853, 854, 855, 856, 857, 858, 1496, 1491}},
        {QStringLiteral("measure-upperarm-circ"),
         {8383, 8393, 8392, 8391, 8390, 8394, 8395, 8399, 10455, 10516,
          8396, 8397, 8398, 8388, 8387, 8386, 10431, 8385, 8384, 8389}},
        {QStringLiteral("measure-upperarm-length"), {8274, 10037}},
        {QStringLiteral("measure-lowerarm-length"), {10040, 10548}},
        {QStringLiteral("measure-wrist-circ"),
         {10208, 10211, 10212, 10216, 10471, 10533, 10213, 10214, 10215, 10205,
          10204, 10203, 10437, 10202, 10201, 10206, 10200, 10210, 10209, 10208}},
        {QStringLiteral("measure-bust-circ"),
         {8439, 8455, 8462, 8446, 8478, 8494, 8557, 8510, 8526, 8542, 10720, 10601, 10603, 10602, 10612,
          10611, 10610, 10613, 10604, 10605, 10606, 3942, 3941, 3940, 3950, 3947, 3948, 3949, 3938, 3939,
          3937, 4065, 1870, 1854, 1838, 1885, 1822, 1806, 1774, 1790, 1783, 1767, 1799, 8471}},
        {QStringLiteral("measure-underbust-circ"),
         {10750, 10744, 10724, 10725, 10748, 10722, 10640, 10642, 10641, 10651, 10650, 10649, 10652, 10643,
          10644, 10645, 10646, 10647, 10648, 3988,  3987,  3986,  3985,  3984,  3983,  3982,  3992,  3989,
          3990,  3991,  3980,  3981,  3979,  4067,  4098,  4073,  4072,  4094,  4100,  4082,  4088,  4088}},
        {QStringLiteral("measure-waist-circ"),
         {4121,  10760, 10757, 10777, 10776, 10779, 10780, 10778, 10781, 10771, 10773, 10772,
          10775, 10774, 10814, 10834, 10816, 10817, 10818, 10819, 10820, 10821, 4181,  4180,
          4179,  4178,  4177,  4176,  4175,  4196,  4173,  4131,  4132,  4129,  4130,  4128,
          4138,  4135,  4137,  4136,  4133,  4134,  4108,  4113,  4118,  4121}},
        {QStringLiteral("measure-napetowaist-dist"), {1491, 4181}},
        {QStringLiteral("measure-waisttohip-dist"), {4121, 4341}},
        {QStringLiteral("measure-shoulder-dist"), {7478, 8274}},
        {QStringLiteral("measure-hips-circ"),
         {4341,  10968, 10969, 10971, 10970, 10967, 10928, 10927, 10925, 10926, 10923, 10924,
          10868, 10875, 10861, 10862, 4228,  4227,  4226,  4242,  4234,  4294,  4293,  4296,
          4295,  4297,  4298,  4342,  4345,  4346,  4344,  4343,  4361,  4341}},
        {QStringLiteral("measure-upperleg-height"), {10970, 11230}},
        {QStringLiteral("measure-thigh-circ"),
         {11071, 11080, 11081, 11086, 11076, 11077, 11074, 11075, 11072, 11073,
          11069, 11070, 11087, 11085, 11084, 12994, 11083, 11082, 11079, 11071}},
        {QStringLiteral("measure-lowerleg-height"), {11225, 12820}},
        {QStringLiteral("measure-calf-circ"),
         {11339, 11336, 11353, 11351, 11350, 13008, 11349, 11348, 11345, 11337,
          11344, 11346, 11347, 11352, 11342, 11343, 11340, 11341, 11338, 11339}},
        {QStringLiteral("measure-ankle-circ"),
         {11460, 11464, 11458, 11459, 11419, 11418, 12958, 12965, 12960, 12963,
          12961, 12962, 12964, 12927, 13028, 12957, 11463, 11461, 11457, 11460}},
        {QStringLiteral("measure-knee-circ"),
         {11223, 11230, 11232, 11233, 11238, 11228, 11229, 11226, 11227, 11224,
          11225, 11221, 11222, 11239, 11237, 11236, 13002, 11235, 11234, 11223}},
    };
    return table;
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Weights of a three valued macro (min, average, max) for a value in [0, 1] (human.py _set*Vals).
 * human.py writes the average as 1 - (max + min) or 1 - max(max, min); both agree because max or min is always 0.
 */
void threeWay(MacroFactors &factors, double value, MacroValue minValue, MacroValue averageValue, MacroValue maxValue)
{
    const double maxVal = qMax(0.0, value * 2 - 1);
    const double minVal = qMax(0.0, 1 - value * 2);
    factors[static_cast<size_t>(maxValue)] = maxVal;
    factors[static_cast<size_t>(minValue)] = minVal;
    factors[static_cast<size_t>(averageValue)] = 1 - (maxVal > minVal ? maxVal : minVal);
}

const double minAge = 1.0;  // human.py MIN_AGE
const double midAge = 25.0; // human.py MID_AGE
const double maxAge = 90.0; // human.py MAX_AGE

} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
MacroFactors MakeHumanMacros::factors() const
{
    MacroFactors f{};
    const auto set = [&f](MacroValue value, double weight) { f[static_cast<size_t>(value)] = weight; };

    const double g = qBound(0.0, gender, 1.0);
    set(MacroValue::Male, g);
    set(MacroValue::Female, 1 - g);

    // 1y baby, 10y child, 25y young, 90y old: age 0, 0.1875, 0.5, 1.
    const double a = qBound(0.0, age, 1.0);
    double baby = 0, child = 0, young = 0, old = 0;
    if (a < 0.5)
    {
        baby = qMax(0.0, 1 - a * 5.333);
        young = qMax(0.0, (a - 0.1875) * 3.2);
        child = qMax(0.0, qMin(1.0, 5.333 * a) - young);
    }
    else
    {
        old = qMax(0.0, a * 2 - 1);
        young = 1 - old;
    }
    set(MacroValue::Baby, baby);
    set(MacroValue::Child, child);
    set(MacroValue::Young, young);
    set(MacroValue::Old, old);

    threeWay(f, qBound(0.0, muscle, 1.0), MacroValue::MinMuscle, MacroValue::AverageMuscle, MacroValue::MaxMuscle);
    threeWay(f, qBound(0.0, weight, 1.0), MacroValue::MinWeight, MacroValue::AverageWeight, MacroValue::MaxWeight);
    threeWay(f, qBound(0.0, height, 1.0), MacroValue::MinHeight, MacroValue::AverageHeight, MacroValue::MaxHeight);
    threeWay(f, qBound(0.0, breastSize, 1.0), MacroValue::MinCup, MacroValue::AverageCup, MacroValue::MaxCup);
    threeWay(f, qBound(0.0, breastFirmness, 1.0), MacroValue::MinFirmness, MacroValue::AverageFirmness,
             MacroValue::MaxFirmness);
    // Proportion targets are not bundled; regular proportions (the default) apply none.
    set(MacroValue::RegularProportions, 1.0);

    // Ethnic values always sum to 1 (human.py _setEthnicVals).
    double races[3] = {qMax(0.0, african), qMax(0.0, asian), qMax(0.0, caucasian)};
    const double total = races[0] + races[1] + races[2];
    for (double &race : races)
    {
        race = total > 0 ? race / total : 1.0 / 3.0;
    }
    set(MacroValue::African, races[0]);
    set(MacroValue::Asian, races[1]);
    set(MacroValue::Caucasian, races[2]);
    return f;
}

//---------------------------------------------------------------------------------------------------------------------
double MakeHumanMacros::factor(MacroValue value) const
{
    return factors().at(static_cast<size_t>(value));
}

//---------------------------------------------------------------------------------------------------------------------
double MakeHumanMacros::ageFromYears(double years)
{
    years = qBound(minAge, years, maxAge);
    if (years < midAge)
    {
        return (years - minAge) / ((midAge - minAge) * 2);
    }
    return (years - midAge) / ((maxAge - midAge) * 2) + 0.5;
}

//---------------------------------------------------------------------------------------------------------------------
double MakeHumanMacros::yearsFromAge(double age)
{
    age = qBound(0.0, age, 1.0);
    if (age < 0.5)
    {
        return minAge + ((midAge - minAge) * 2) * age;
    }
    return midAge + ((maxAge - midAge) * 2) * (age - 0.5);
}

//---------------------------------------------------------------------------------------------------------------------
QStringList MakeHumanData::targetFactors(const QString &name)
{
    QStringList result;
    for (const int id : factorIds(name))
    {
        result.append(QString::fromLatin1(macroValueNames[id]));
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
QStringList MakeHumanData::modifierNames()
{
    QStringList names;
    for (const ModifierSpec &spec : modifierSpecs())
    {
        names.append(spec.name);
    }
    return names;
}

//---------------------------------------------------------------------------------------------------------------------
bool MakeHumanData::load(QIODevice *device)
{
    m_errorString.clear();
    basePositions.clear();
    triangles.clear();
    targets.clear();
    modifiers.clear();
    m_index.clear();

    const auto fail = [this](const QString &error)
    {
        m_errorString = error;
        basePositions.clear();
        triangles.clear();
        targets.clear();
        modifiers.clear();
        m_index.clear();
        return false;
    };

    const QByteArray data = qUncompress(device->readAll());
    if (data.isEmpty())
    {
        return fail(tr("The MakeHuman body data is damaged."));
    }

    QDataStream in(data);
    in.setByteOrder(QDataStream::LittleEndian);
    in.setFloatingPointPrecision(QDataStream::SinglePrecision);

    char magic[4];
    if (in.readRawData(magic, 4) != 4 || memcmp(magic, packMagic, 4) != 0)
    {
        return fail(tr("Not a MakeHuman body data pack."));
    }

    quint32 version = 0;
    in >> version;
    if (version != packVersion)
    {
        return fail(tr("Unsupported MakeHuman body data version %1.").arg(version));
    }

    float deltaScale = 0;
    quint32 vertexCount = 0;
    in >> deltaScale >> vertexCount;
    if (!(deltaScale > 0) || vertexCount == 0 || vertexCount > std::numeric_limits<quint16>::max())
    {
        return fail(tr("The MakeHuman body data is damaged."));
    }

    basePositions.resize(static_cast<int>(vertexCount));
    for (QVector3D &position : basePositions)
    {
        float x = 0, y = 0, z = 0;
        in >> x >> y >> z;
        position = QVector3D(x, y, z);
    }

    quint32 quadCount = 0;
    in >> quadCount;
    triangles.reserve(static_cast<int>(quadCount) * 6);
    for (quint32 i = 0; i < quadCount && in.status() == QDataStream::Ok; ++i)
    {
        quint16 q[4];
        in >> q[0] >> q[1] >> q[2] >> q[3];
        for (const quint16 index : q)
        {
            if (index >= vertexCount)
            {
                return fail(tr("The MakeHuman body data is damaged."));
            }
        }
        triangles << q[0] << q[1] << q[2] << q[0] << q[2] << q[3];
    }

    quint32 targetCount = 0;
    in >> targetCount;
    for (quint32 t = 0; t < targetCount && in.status() == QDataStream::Ok; ++t)
    {
        MakeHumanTarget target;

        quint16 nameLength = 0;
        in >> nameLength;
        QByteArray name(nameLength, Qt::Uninitialized);
        in.readRawData(name.data(), nameLength);
        target.name = QString::fromUtf8(name);
        target.factors = factorIds(target.name);

        quint32 entries = 0;
        in >> entries;
        if (entries > vertexCount)
        {
            return fail(tr("The MakeHuman body data is damaged."));
        }

        // Indices are stored as gaps from the previous index; deltas as separate x, y and z planes.
        target.indices.resize(static_cast<int>(entries));
        quint32 index = 0;
        for (quint16 &stored : target.indices)
        {
            quint16 gap = 0;
            in >> gap;
            index += gap;
            if (index >= vertexCount)
            {
                return fail(tr("The MakeHuman body data is damaged."));
            }
            stored = static_cast<quint16>(index);
        }

        target.deltas.resize(static_cast<int>(entries));
        const float factor = 1.0f / deltaScale;
        for (int axis = 0; axis < 3; ++axis)
        {
            for (QVector3D &delta : target.deltas)
            {
                qint16 value = 0;
                in >> value;
                delta[axis] = static_cast<float>(value) * factor;
            }
        }

        m_index.insert(target.name, static_cast<int>(targets.size()));
        targets.append(target);
    }

    if (in.status() != QDataStream::Ok || triangles.isEmpty())
    {
        return fail(tr("The MakeHuman body data is damaged."));
    }

    if (!buildModifiers())
    {
        return fail(m_errorString);
    }
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
bool MakeHumanData::buildModifiers()
{
    const int vertexCount = static_cast<int>(basePositions.size());

    const auto dense = [this, vertexCount](const QStringList &names, QVector<QVector3D> &deltas)
    {
        deltas = QVector<QVector3D>(vertexCount);
        for (const QString &name : names)
        {
            const int index = indexOf(name);
            if (index < 0)
            {
                m_errorString = tr("The MakeHuman body data has no target %1.").arg(name);
                return false;
            }
            const MakeHumanTarget &target = targets.at(index);
            for (int i = 0; i < target.indices.size(); ++i)
            {
                deltas[target.indices.at(i)] += target.deltas.at(i);
            }
        }
        return true;
    };

    for (const ModifierSpec &spec : modifierSpecs())
    {
        MakeHumanModifier modifier;
        modifier.name = spec.name;
        if (!dense(spec.decrease, modifier.decrease) || !dense(spec.increase, modifier.increase))
        {
            return false;
        }
        modifiers.append(modifier);
    }
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
QSharedPointer<const MakeHumanData> MakeHumanData::instance(QString *error)
{
    static QMutex mutex;
    static QSharedPointer<const MakeHumanData> loaded;
    static QString loadError;
    static bool attempted = false;

    QMutexLocker locker(&mutex);
    if (!attempted)
    {
        attempted = true;
        initMakeHumanResource();

        QFile file(resourcePath);
        auto data = QSharedPointer<MakeHumanData>::create();
        if (!file.open(QIODevice::ReadOnly))
        {
            loadError = tr("The MakeHuman body data is missing.");
        }
        else if (!data->load(&file))
        {
            loadError = data->errorString();
        }
        else
        {
            loaded = data;
        }
    }

    if (error != nullptr)
    {
        *error = loadError;
    }
    return loaded;
}

//---------------------------------------------------------------------------------------------------------------------
int MakeHumanData::indexOf(const QString &name) const
{
    return m_index.value(name, -1);
}

//---------------------------------------------------------------------------------------------------------------------
QString MakeHumanData::errorString() const
{
    return m_errorString;
}

//---------------------------------------------------------------------------------------------------------------------
MakeHumanBody::MakeHumanBody(const QSharedPointer<const MakeHumanData> &data)
    : m_data(data)
    , m_values(data->modifiers.size(), 0.0)
{
    Q_ASSERT(m_data);
    setMacros(MakeHumanMacros());
}

//---------------------------------------------------------------------------------------------------------------------
void MakeHumanBody::setMacros(const MakeHumanMacros &macros)
{
    m_macros = macros;
    m_macroPositions = m_data->basePositions;

    const MacroFactors factors = macros.factors();
    for (const MakeHumanTarget &target : m_data->targets)
    {
        if (target.factors.isEmpty())
        {
            continue; // modifier target
        }

        // The weight of a macro target is the product of the macro values it is made for (getTargetWeights).
        double weight = 1;
        for (const int factor : target.factors)
        {
            weight *= factors.at(static_cast<size_t>(factor));
        }
        if (weight < 1e-6)
        {
            continue;
        }

        const auto w = static_cast<float>(weight);
        for (int i = 0; i < target.indices.size(); ++i)
        {
            m_macroPositions[target.indices.at(i)] += target.deltas.at(i) * w;
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
MakeHumanMacros MakeHumanBody::macros() const
{
    return m_macros;
}

//---------------------------------------------------------------------------------------------------------------------
void MakeHumanBody::setModifier(const QString &name, double value)
{
    const int index = static_cast<int>(MakeHumanData::modifierNames().indexOf(name));
    if (index >= 0 && index < m_values.size())
    {
        QVector<double> values = m_values;
        values[index] = value;
        setModifierValues(values);
    }
}

//---------------------------------------------------------------------------------------------------------------------
double MakeHumanBody::modifier(const QString &name) const
{
    const int index = static_cast<int>(MakeHumanData::modifierNames().indexOf(name));
    return index >= 0 && index < m_values.size() ? m_values.at(index) : 0.0;
}

//---------------------------------------------------------------------------------------------------------------------
void MakeHumanBody::setModifierValues(const QVector<double> &values)
{
    m_active.clear();
    for (int i = 0; i < m_values.size(); ++i)
    {
        m_values[i] = i < values.size() ? qBound(-modifierLimit, values.at(i), modifierLimit) : 0.0;
        if (!qFuzzyIsNull(m_values.at(i)))
        {
            m_active.append(i);
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
QVector<double> MakeHumanBody::modifierValues() const
{
    return m_values;
}

//---------------------------------------------------------------------------------------------------------------------
QStringList MakeHumanBody::rulerNames()
{
    QStringList names = rulers().keys();
    names.sort();
    return names;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<int> MakeHumanBody::ruler(const QString &name)
{
    return rulers().value(name);
}

//---------------------------------------------------------------------------------------------------------------------
QVector3D MakeHumanBody::position(int vertex) const
{
    QVector3D p = m_macroPositions.at(vertex);
    for (const int index : m_active)
    {
        const double value = m_values.at(index);
        const MakeHumanModifier &modifier = m_data->modifiers.at(index);
        p += (value < 0 ? modifier.decrease : modifier.increase).at(vertex) * static_cast<float>(qAbs(value));
    }
    return p;
}

//---------------------------------------------------------------------------------------------------------------------
double MakeHumanBody::measure(const QString &ruler) const
{
    const QVector<int> vertices = rulers().value(ruler);
    double length = 0;
    for (int i = 1; i < vertices.size(); ++i)
    {
        length += static_cast<double>((position(vertices.at(i)) - position(vertices.at(i - 1))).length());
    }
    return length * 10.0; // decimeters to centimeters
}

//---------------------------------------------------------------------------------------------------------------------
QVector<QVector3D> MakeHumanBody::positions() const
{
    QVector<QVector3D> result = m_macroPositions;
    for (const int index : m_active)
    {
        const double value = m_values.at(index);
        const MakeHumanModifier &modifier = m_data->modifiers.at(index);
        const QVector<QVector3D> &deltas = value < 0 ? modifier.decrease : modifier.increase;
        const auto weight = static_cast<float>(qAbs(value));
        for (int i = 0; i < result.size(); ++i)
        {
            result[i] += deltas.at(i) * weight;
        }
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
double MakeHumanBody::heightCm() const
{
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    for (const QVector3D &p : positions())
    {
        minY = qMin(minY, p.y());
        maxY = qMax(maxY, p.y());
    }
    return static_cast<double>(maxY - minY) * 10.0;
}

//---------------------------------------------------------------------------------------------------------------------
Mesh MakeHumanBody::mesh(double scale) const
{
    Mesh mesh;
    mesh.positions = positions();

    const auto factor = static_cast<float>(10.0 * scale);
    float minY = std::numeric_limits<float>::max();
    for (QVector3D &p : mesh.positions)
    {
        p *= factor;
        minY = qMin(minY, p.y());
    }
    mesh.translate(QVector3D(0, -minY, 0));

    mesh.indices = m_data->triangles;
    mesh.computeNormals();
    return mesh;
}

//---------------------------------------------------------------------------------------------------------------------
const MakeHumanData &MakeHumanBody::data() const
{
    return *m_data;
}

} // namespace Seamly3D
