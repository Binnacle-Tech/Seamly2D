/******************************************************************************
 *   @file   patternsource.h
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

#ifndef SEAMLY3D_PATTERNSOURCE_H
#define SEAMLY3D_PATTERNSOURCE_H

#include <QCoreApplication>
#include <QMetaObject>
#include <QStringList>
#include <QVector>

#include <functional>

#include "bodymeasurements.h"
#include "patternprovider.h"
#include "piecemesher.h"

class QObject;

namespace Seamly3D
{

/**
 * @brief Read only access to the pattern open in Seamly2D: its pieces and the measurements it is drafted for.
 *
 * This is the one place where the 3D workspace reads 2D data. It only reads, through the application's current
 * data container and document, so the 2D code needs no changes.
 */
class PatternSource
{
    Q_DECLARE_TR_FUNCTIONS(Seamly3D::PatternSource)

public:
    /** @brief pieces returns the pieces marked "in layout", their seam lines in centimeters. */
    static QVector<PatternPiece> pieces(QString *error = nullptr);

    /**
     * @brief applyMeasurements sets the body measurements the pattern uses (from its measurement file) on
     * @p measurements, keeping its gender, age and muscle. Measurements the pattern does not have follow its height.
     * @return the names of the measurements taken from the pattern; empty when it has none of them.
     */
    static QStringList applyMeasurements(BodyMeasurements &measurements);

    /** @brief watch calls @p changed whenever the pattern changes, until @p context is destroyed. */
    static QMetaObject::Connection watch(QObject *context, const std::function<void()> &changed);

    /** @brief defaultUnit is the unit chosen in Seamly2D's preferences (asked on first start): "cm", "mm" or "inch". */
    static QString defaultUnit();

    /** @brief patternFile is the file the pattern was opened from or last saved to; empty for a new pattern. */
    static QString patternFile();
};

/**
 * @brief The provider of the pattern open in Seamly2D, for the 3D workspace window.
 */
class OpenPatternProvider : public PatternProvider
{
public:
    QVector<PatternPiece> pieces(QString *error) override { return PatternSource::pieces(error); }
    QStringList applyMeasurements(BodyMeasurements &measurements) override
    {
        return PatternSource::applyMeasurements(measurements);
    }
    QMetaObject::Connection watch(QObject *context, const std::function<void()> &changed) override
    {
        return PatternSource::watch(context, changed);
    }
    QString defaultUnit() const override { return PatternSource::defaultUnit(); }
    QString patternFile() const override { return PatternSource::patternFile(); }
};

} // namespace Seamly3D

#endif // SEAMLY3D_PATTERNSOURCE_H
