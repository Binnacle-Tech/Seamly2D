/******************************************************************************
 *   @file   makehumanfitter.cpp
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

#include "makehumanfitter.h"

#include <QtMath>

#include <functional>
#include <limits>

namespace Seamly3D
{

namespace
{

//---------------------------------------------------------------------------------------------------------------------
/** @brief A body measurement and the MakeHuman modifiers (moved together) and rulers (summed) that control it. */
struct Control
{
    BodyMeasurement measurement;
    QStringList     modifiers;
    QStringList     rulers;
};

const QVector<Control> &controls()
{
    static const QVector<Control> table = {
        {BodyMeasurement::NeckCirc, {QStringLiteral("measure-neck-circ")}, {QStringLiteral("measure-neck-circ")}},
        {BodyMeasurement::BustCirc, {QStringLiteral("measure-bust-circ")}, {QStringLiteral("measure-bust-circ")}},
        {BodyMeasurement::UnderbustCirc,
         {QStringLiteral("measure-underbust-circ")},
         {QStringLiteral("measure-underbust-circ")}},
        {BodyMeasurement::WaistCirc, {QStringLiteral("measure-waist-circ")}, {QStringLiteral("measure-waist-circ")}},
        {BodyMeasurement::HipCirc, {QStringLiteral("measure-hips-circ")}, {QStringLiteral("measure-hips-circ")}},
        {BodyMeasurement::ThighCirc, {QStringLiteral("measure-thigh-circ")}, {QStringLiteral("measure-thigh-circ")}},
        {BodyMeasurement::KneeCirc, {QStringLiteral("measure-knee-circ")}, {QStringLiteral("measure-knee-circ")}},
        {BodyMeasurement::CalfCirc, {QStringLiteral("measure-calf-circ")}, {QStringLiteral("measure-calf-circ")}},
        {BodyMeasurement::AnkleCirc, {QStringLiteral("measure-ankle-circ")}, {QStringLiteral("measure-ankle-circ")}},
        {BodyMeasurement::UpperArmCirc,
         {QStringLiteral("measure-upperarm-circ")},
         {QStringLiteral("measure-upperarm-circ")}},
        {BodyMeasurement::WristCirc, {QStringLiteral("measure-wrist-circ")}, {QStringLiteral("measure-wrist-circ")}},
        {BodyMeasurement::ArmLength,
         {QStringLiteral("measure-upperarm-length"), QStringLiteral("measure-lowerarm-length")},
         {QStringLiteral("measure-upperarm-length"), QStringLiteral("measure-lowerarm-length")}},
    };
    return table;
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief bisect finds x in [low, high] where f(x) = 0, for f monotonic in either direction. When there is no root in
 * the range it returns the end that comes closest.
 */
double bisect(const std::function<double(double)> &f, double low, double high, double tolerance, int iterations = 40)
{
    double fLow = f(low);
    double fHigh = f(high);
    if ((fLow > 0) == (fHigh > 0))
    {
        return qAbs(fLow) < qAbs(fHigh) ? low : high;
    }

    for (int i = 0; i < iterations; ++i)
    {
        const double middle = (low + high) / 2;
        const double fMiddle = f(middle);
        if (qAbs(fMiddle) < tolerance)
        {
            return middle;
        }
        if ((fMiddle > 0) == (fLow > 0))
        {
            low = middle;
            fLow = fMiddle;
        }
        else
        {
            high = middle;
        }
    }
    return (low + high) / 2;
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief clampedStep solves jacobian * delta = residual (Gaussian elimination with partial pivoting) and returns
 * values + delta clamped to [-limit, limit]. Measures with no usable column keep their value.
 */
QVector<double> clampedStep(QVector<QVector<double>> a, QVector<double> b, const QVector<double> &values, double limit)
{
    const int n = static_cast<int>(b.size());
    QVector<bool> solvable(n, true);

    for (int column = 0; column < n; ++column)
    {
        int pivot = column;
        for (int row = column + 1; row < n; ++row)
        {
            if (qAbs(a.at(row).at(column)) > qAbs(a.at(pivot).at(column)))
            {
                pivot = row;
            }
        }
        if (qAbs(a.at(pivot).at(column)) < 1e-9)
        {
            solvable[column] = false;
            continue;
        }
        std::swap(a[column], a[pivot]);
        std::swap(b[column], b[pivot]);

        for (int row = column + 1; row < n; ++row)
        {
            const double factor = a.at(row).at(column) / a.at(column).at(column);
            for (int k = column; k < n; ++k)
            {
                a[row][k] -= factor * a.at(column).at(k);
            }
            b[row] -= factor * b.at(column);
        }
    }

    QVector<double> delta(n, 0.0);
    for (int row = n - 1; row >= 0; --row)
    {
        if (!solvable.at(row))
        {
            continue;
        }
        double sum = b.at(row);
        for (int k = row + 1; k < n; ++k)
        {
            sum -= a.at(row).at(k) * delta.at(k);
        }
        delta[row] = sum / a.at(row).at(row);
    }

    QVector<double> result(n);
    for (int i = 0; i < n; ++i)
    {
        result[i] = qBound(-limit, values.at(i) + delta.at(i), limit);
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
double rulerLength(const MakeHumanBody &body, const QStringList &rulers)
{
    double length = 0;
    for (const QString &ruler : rulers)
    {
        length += body.measure(ruler);
    }
    return length;
}

} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
double MakeHumanFit::maxError() const
{
    double error = 0;
    for (const Entry &entry : entries)
    {
        if (entry.fitted)
        {
            error = qMax(error, qAbs(entry.achieved - entry.target));
        }
    }
    return error;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<BodyMeasurement> MakeHumanFit::unfitted() const
{
    QVector<BodyMeasurement> result;
    for (const Entry &entry : entries)
    {
        if (!entry.fitted)
        {
            result.append(entry.measurement);
        }
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
MakeHumanFit MakeHumanFitter::fit(const BodyMeasurements &measurements, const QSharedPointer<const MakeHumanData> &data)
{
    MakeHumanFit result;
    if (data.isNull())
    {
        result.errorString = tr("The MakeHuman body data is not available.");
        return result;
    }

    const BodyMeasurements m = measurements.sanitized();
    const double targetHeight = m.value(BodyMeasurement::Height);

    MakeHumanBody body(data);
    MakeHumanMacros macros;
    macros.gender = m.gender();
    macros.muscle = m.muscle();

    const auto scaleFor = [&body, targetHeight]() { return targetHeight / body.heightCm(); };
    const auto scaledLength = [&body, &scaleFor](const QStringList &rulers)
    { return scaleFor() * rulerLength(body, rulers); };

    const auto fitHeightMacro = [&]()
    {
        macros.height = bisect(
            [&](double value)
            {
                macros.height = value;
                body.setMacros(macros);
                return body.heightCm() - targetHeight;
            },
            0.0, 1.0, 0.1);
        body.setMacros(macros);
    };

    // ---- Macros: overall height and build. -------------------------------------------------------------------------
    fitHeightMacro();

    const QStringList bust = {QStringLiteral("measure-bust-circ")};
    const QStringList underbust = {QStringLiteral("measure-underbust-circ")};
    const QStringList waist = {QStringLiteral("measure-waist-circ")};
    const QStringList hips = {QStringLiteral("measure-hips-circ")};

    macros.weight = bisect(
        [&](double value)
        {
            macros.weight = value;
            body.setMacros(macros);
            const double ratio = (scaledLength(bust) / m.value(BodyMeasurement::BustCirc)
                                  + scaledLength(waist) / m.value(BodyMeasurement::WaistCirc)
                                  + scaledLength(hips) / m.value(BodyMeasurement::HipCirc))
                                 / 3.0;
            return ratio - 1.0;
        },
        0.0, 1.0, 1e-3);
    body.setMacros(macros);
    fitHeightMacro();

    if (macros.gender < 0.5)
    {
        const double wantedDifference = m.value(BodyMeasurement::BustCirc) - m.value(BodyMeasurement::UnderbustCirc);
        macros.breastSize = bisect(
            [&](double value)
            {
                macros.breastSize = value;
                body.setMacros(macros);
                return (scaledLength(bust) - scaledLength(underbust)) - wantedDifference;
            },
            0.0, 1.0, 0.1);
        body.setMacros(macros);
    }

    // ---- Measure modifiers: all girths at once. --------------------------------------------------------------------
    // Neighbouring modifiers change each other's measures (bust and lowbust a lot), so instead of fitting them one by
    // one, Newton steps solve them together. The measures are nearly linear in the modifier values.
    const QVector<Control> &table = controls();
    const int n = static_cast<int>(table.size());
    QVector<double> values(n, 0.0);

    const auto apply = [&](const QVector<double> &v)
    {
        for (int i = 0; i < n; ++i)
        {
            for (const QString &modifier : table.at(i).modifiers)
            {
                body.setModifier(modifier, v.at(i));
            }
        }
    };
    const auto residuals = [&](double scale)
    {
        QVector<double> r(n);
        for (int i = 0; i < n; ++i)
        {
            r[i] = m.value(table.at(i).measurement) - scale * rulerLength(body, table.at(i).rulers);
        }
        return r;
    };

    for (int iteration = 0; iteration < 12; ++iteration)
    {
        apply(values);
        const double scale = scaleFor();
        const QVector<double> r = residuals(scale);

        double worst = 0;
        for (const double residual : r)
        {
            worst = qMax(worst, qAbs(residual));
        }
        if (worst < tolerance / 2)
        {
            break;
        }

        // Jacobian by forward differences: column j is how every measure moves with modifier j.
        const double step = 0.05;
        QVector<QVector<double>> jacobian(n, QVector<double>(n));
        for (int j = 0; j < n; ++j)
        {
            QVector<double> moved = values;
            moved[j] += values.at(j) + step <= MakeHumanBody::modifierLimit ? step : -step;
            const double h = moved.at(j) - values.at(j);
            apply(moved);
            const QVector<double> rMoved = residuals(scale);
            for (int i = 0; i < n; ++i)
            {
                jacobian[i][j] = (r.at(i) - rMoved.at(i)) / h; // d(measure)/d(value)
            }
        }

        values = clampedStep(jacobian, r, values, MakeHumanBody::modifierLimit);
    }
    apply(values);

    // ---- Result. ---------------------------------------------------------------------------------------------------
    result.scale = scaleFor();
    result.macros = macros;
    result.mesh = body.mesh(result.scale);

    MakeHumanFit::Entry height;
    height.measurement = BodyMeasurement::Height;
    height.target = targetHeight;
    height.achieved = static_cast<double>(result.mesh.bounds().size().y());
    height.fitted = true;
    result.entries.append(height);

    for (const Control &control : controls())
    {
        MakeHumanFit::Entry entry;
        entry.measurement = control.measurement;
        entry.target = m.value(control.measurement);
        entry.achieved = result.scale * rulerLength(body, control.rulers);
        entry.fitted = true;
        result.entries.append(entry);
    }

    for (int i = 0; i < BodyMeasurements::count(); ++i)
    {
        const auto measurement = static_cast<BodyMeasurement>(i);
        bool controlled = measurement == BodyMeasurement::Height;
        for (const Control &control : controls())
        {
            controlled = controlled || control.measurement == measurement;
        }
        if (!controlled)
        {
            MakeHumanFit::Entry entry;
            entry.measurement = measurement;
            entry.target = m.value(measurement);
            entry.achieved = std::numeric_limits<double>::quiet_NaN();
            entry.fitted = false;
            result.entries.append(entry);
        }
    }

    result.valid = true;
    return result;
}

} // namespace Seamly3D
