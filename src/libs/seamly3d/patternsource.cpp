/******************************************************************************
 *   @file   patternsource.cpp
 **  @author Seamly2D project
 **  @date   15 10, 2026
 **
 **  @brief  Read only access to the pattern open in Seamly2D.
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

#include "patternsource.h"

#include <QHash>
#include <QLineF>
#include <QSharedPointer>

#include "../ifc/xml/vabstractpattern.h"
#include "../vmisc/def.h"
#include "../vmisc/vabstractapplication.h"
#include "../vmisc/vcommonsettings.h"
#include "../vpatterndb/floatItemData/vpiecelabeldata.h"
#include "../vpatterndb/variables/measurement_variable.h"
#include "../vpatterndb/vcontainer.h"
#include "../vgeometry/vpointf.h"
#include "../vpatterndb/vpiece.h"

namespace Seamly3D
{

//---------------------------------------------------------------------------------------------------------------------
QVector<PatternPiece> PatternSource::pieces(QString *error)
{
    QVector<PatternPiece> result;
    const VContainer *data = qApp->getCurrentData();
    if (data == nullptr)
    {
        if (error != nullptr)
        {
            *error = tr("No pattern is open.");
        }
        return result;
    }

    QStringList skipped;
    const QHash<quint32, VPiece> *pieces = data->DataPieces();
    for (auto it = pieces->constBegin(); it != pieces->constEnd(); ++it)
    {
        const VPiece &piece = it.value();
        if (!piece.isInLayout())
        {
            continue;
        }

        try
        {
            PatternPiece result3D;
            result3D.id = it.key();
            result3D.name = piece.GetName();
            for (const QPointF &point : piece.mainPathPoints(data))
            {
                // Pattern coordinates are pixels; the 3D workspace works in centimeters.
                result3D.seamLine.append(QPointF(FromPixel(point.x(), Unit::Cm), FromPixel(point.y(), Unit::Cm)));
            }
            // The piece's own points (where its seam lines meet) become the corners that split it into edges.
            for (const VPointF &node : piece.mainPathNodePoints(data))
            {
                const QPointF corner(FromPixel(node.x(), Unit::Cm), FromPixel(node.y(), Unit::Cm));
                int nearest = -1;
                qreal nearestDistance = 0.05; // centimeters
                for (int i = 0; i < result3D.seamLine.size(); ++i)
                {
                    const qreal distance = QLineF(result3D.seamLine.at(i), corner).length();
                    if (distance < nearestDistance)
                    {
                        nearestDistance = distance;
                        nearest = i;
                    }
                }
                if (nearest >= 0 && !result3D.corners.contains(nearest))
                {
                    result3D.corners.append(nearest);
                }
            }
            std::sort(result3D.corners.begin(), result3D.corners.end());

            const VPieceLabelData &label = piece.GetPatternPieceData();
            result3D.quantity = qMax(1, label.GetQuantity());
            result3D.onFold = label.IsOnFold();
            if (result3D.seamLine.size() >= 3)
            {
                result.append(result3D);
            }
        }
        catch (...)
        {
            skipped.append(piece.GetName()); // a piece the 2D side can not build either
        }
    }

    // Stable order: the order the pieces were made in.
    std::sort(result.begin(), result.end(), [](const PatternPiece &a, const PatternPiece &b) { return a.id < b.id; });

    if (error != nullptr)
    {
        *error = skipped.isEmpty() ? QString() : tr("Could not read: %1").arg(skipped.join(QLatin1String(", ")));
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
QStringList PatternSource::applyMeasurements(BodyMeasurements &measurements)
{
    const VContainer *data = qApp->getCurrentData();
    if (data == nullptr)
    {
        return {};
    }

    QHash<QString, double> found;
    const auto patternMeasurements = data->DataMeasurements();
    for (auto it = patternMeasurements.constBegin(); it != patternMeasurements.constEnd(); ++it)
    {
        const double value = UnitConvertor(*it.value()->GetValue(), qApp->patternUnit(), Unit::Cm);
        if (value > 0)
        {
            found.insert(it.key(), value);
        }
    }

    // Start from the default body of the person's height, then use what the pattern has.
    BodyMeasurements result = BodyMeasurements::defaults(measurements.gender());
    result.setMuscle(measurements.muscle());
    result.setAge(measurements.age());

    const QString height = BodyMeasurements::name(BodyMeasurement::Height);
    if (found.contains(height))
    {
        result.scaleToHeight(found.value(height));
    }

    QStringList applied;
    for (int i = 0; i < BodyMeasurements::count(); ++i)
    {
        const auto measurement = static_cast<BodyMeasurement>(i);
        const auto it = found.constFind(BodyMeasurements::name(measurement));
        if (it != found.constEnd())
        {
            result.setValue(measurement, it.value());
            applied.append(it.key());
        }
    }

    if (!applied.isEmpty())
    {
        measurements = result;
    }
    return applied;
}

//---------------------------------------------------------------------------------------------------------------------
QMetaObject::Connection PatternSource::watch(QObject *context, const std::function<void()> &changed)
{
    VAbstractPattern *document = qApp->getCurrentDocument();
    if (document == nullptr)
    {
        return {};
    }
    return QObject::connect(document, &VAbstractPattern::patternChanged, context, [changed]() { changed(); });
}

//---------------------------------------------------------------------------------------------------------------------
QString PatternSource::defaultUnit()
{
    return qApp->Settings()->getUnit();
}

} // namespace Seamly3D
