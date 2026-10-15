/******************************************************************************
 *   @file   makehumanmodel.h
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

#ifndef SEAMLY3D_MAKEHUMANMODEL_H
#define SEAMLY3D_MAKEHUMANMODEL_H

#include <QCoreApplication>
#include <QHash>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QVector3D>

#include <array>

#include "../mesh.h"

class QIODevice;

namespace Seamly3D
{

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief The macro values a target can depend on (lib/targets.py, _cat_data).
 */
enum class MacroValue : int
{
    Male, Female,
    Baby, Child, Young, Old,
    Caucasian, Asian, African,
    MinMuscle, AverageMuscle, MaxMuscle,
    MinWeight, AverageWeight, MaxWeight,
    MinHeight, AverageHeight, MaxHeight,
    MinCup, AverageCup, MaxCup,
    MinFirmness, AverageFirmness, MaxFirmness,
    UncommonProportions, RegularProportions, IdealProportions,
    Count
};

using MacroFactors = std::array<double, static_cast<size_t>(MacroValue::Count)>;

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief A morph target: per vertex offsets added to the base mesh with a weight.
 */
struct MakeHumanTarget
{
    QString            name{};      ///< e.g. "macrodetails/universal-female-young-averagemuscle-averageweight"
    QVector<int>       factors{};   ///< MacroValue ids the weight depends on; empty for modifier targets
    QVector<quint16>   indices{};
    QVector<QVector3D> deltas{};    ///< Decimeters, like the MakeHuman base mesh.
};

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief A modifier: a decrease and an increase target (or several, moved together) driven by one value.
 */
struct MakeHumanModifier
{
    QString            name{};
    QVector<QVector3D> decrease{}; ///< Dense deltas over all vertices, applied with weight -value for value < 0
    QVector<QVector3D> increase{}; ///< Dense deltas over all vertices, applied with weight value for value > 0
};

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief The MakeHuman base body, targets and modifiers, loaded from the bundled data pack (CC0 assets).
 */
class MakeHumanData
{
    Q_DECLARE_TR_FUNCTIONS(Seamly3D::MakeHumanData)

public:
    MakeHumanData() = default;

    bool load(QIODevice *device);

    /** @brief instance returns the bundled data, loaded on first use. Null if it can not be loaded. */
    static QSharedPointer<const MakeHumanData> instance(QString *error = nullptr);

    QVector<QVector3D>         basePositions{};  ///< Decimeters, Y up, facing +Z.
    QVector<quint32>           triangles{};      ///< The body quads split in two triangles each.
    QVector<MakeHumanTarget>   targets{};
    QVector<MakeHumanModifier> modifiers{};      ///< See modifierNames(); same order.

    int     indexOf(const QString &name) const;
    QString errorString() const;

    /** @brief targetFactors extracts the macro values a target depends on from its name (like targets.py). */
    static QStringList targetFactors(const QString &name);

    /** @brief modifierNames lists the modifiers: MakeHuman's measure modifiers and the modeling ones the fitter uses. */
    static QStringList modifierNames();

private:
    bool buildModifiers();

    QHash<QString, int> m_index{};
    QString             m_errorString{};
};

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Macro parameters of a MakeHuman body, all in [0, 1] (see human.py).
 */
struct MakeHumanMacros
{
    double gender{0.5};          ///< 0 female, 1 male
    double age{0.5};             ///< 0 = 1 year, 0.1875 = 10 years, 0.5 = 25 years, 1 = 90 years
    double muscle{0.5};
    double weight{0.5};
    double height{0.5};
    double breastSize{0.5};
    double breastFirmness{0.5};
    double african{1.0 / 3.0};
    double asian{1.0 / 3.0};
    double caucasian{1.0 / 3.0};

    /** @brief factors returns the weight of every macro value, as human.py computes them. */
    MacroFactors factors() const;

    double factor(MacroValue value) const;

    static double ageFromYears(double years); ///< human.py setAgeYears
    static double yearsFromAge(double age);   ///< human.py getAgeYears
};

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief A MakeHuman body: the base mesh with macro targets and modifiers applied.
 *
 * Modifier values are clamped to [-modifierLimit, modifierLimit]: negative values apply the decrease targets,
 * positive values the increase targets, as humanmodifier.py does. MakeHuman's sliders stop at -1 and 1; going further
 * extrapolates the targets linearly, so measurements a little outside MakeHuman's range can be reached.
 */
class MakeHumanBody
{
public:
    explicit MakeHumanBody(const QSharedPointer<const MakeHumanData> &data);

    static constexpr double modifierLimit = 1.5;

    void            setMacros(const MakeHumanMacros &macros);
    MakeHumanMacros macros() const;

    void            setModifier(const QString &name, double value);
    double          modifier(const QString &name) const;
    void            setModifierValues(const QVector<double> &values); ///< In MakeHumanData::modifierNames() order.
    QVector<double> modifierValues() const;

    /** @brief measure returns a tape measure of the body in centimeters, along a MakeHuman ruler (see rulerNames()). */
    double          measure(const QString &ruler) const;
    static QStringList rulerNames();
    static QVector<int> ruler(const QString &name);

    /** @brief heightCm returns the height of the body in centimeters (bounding box, like human.py). */
    double          heightCm() const;

    QVector3D       position(int vertex) const;     ///< decimeters
    QVector<QVector3D> positions() const;           ///< decimeters, all body vertices

    /**
     * @brief mesh returns the body in centimeters, scaled by @p scale, standing on the floor (y = 0) and facing +Z.
     */
    Mesh            mesh(double scale = 1.0) const;

    const MakeHumanData &data() const;

private:
    QSharedPointer<const MakeHumanData> m_data;
    MakeHumanMacros    m_macros{};
    QVector<QVector3D> m_macroPositions{};
    QVector<double>    m_values{};
    QVector<int>       m_active{}; ///< Indices of modifiers with a non zero value
};

} // namespace Seamly3D

#endif // SEAMLY3D_MAKEHUMANMODEL_H
