/******************************************************************************
 *   @file   patternprovider.h
 **  @author Seamly2D project
 **  @date   15 10, 2026
 **
 **  @brief  Where the 3D workspace gets the pattern from.
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

#ifndef SEAMLY3D_PATTERNPROVIDER_H
#define SEAMLY3D_PATTERNPROVIDER_H

#include <QMetaObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

#include "bodymeasurements.h"
#include "piecemesher.h"

class QObject;

namespace Seamly3D
{

/**
 * @brief Where the 3D workspace gets the pattern from. The application provides the open Seamly2D pattern
 * (PatternSource); without a provider the workspace shows only the body. Tests can provide their own pieces.
 */
class PatternProvider
{
public:
    virtual ~PatternProvider() = default;

    virtual QVector<PatternPiece>   pieces(QString *error) = 0;
    virtual QStringList             applyMeasurements(BodyMeasurements &measurements) = 0;
    virtual QMetaObject::Connection watch(QObject *context, const std::function<void()> &changed) = 0;

    /** @brief defaultUnit is the unit the user works in: "cm", "mm" or "inch". */
    virtual QString                 defaultUnit() const { return QStringLiteral("cm"); }
};

} // namespace Seamly3D

#endif // SEAMLY3D_PATTERNPROVIDER_H
