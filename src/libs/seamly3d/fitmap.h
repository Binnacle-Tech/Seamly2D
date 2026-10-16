/******************************************************************************
 *   @file   fitmap.h
 **  @author Seamly2D project
 **  @date   16 10, 2026
 **
 **  @brief  Colors showing how a garment fits: stretch and ease.
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

#ifndef SEAMLY3D_FITMAP_H
#define SEAMLY3D_FITMAP_H

#include <QColor>
#include <QCoreApplication>
#include <QImage>
#include <QString>
#include <QVector>

namespace Seamly3D
{

/**
 * @brief Colors the draped garment by how it fits.
 *
 * Stretch: how much longer the fabric is than the flat piece, blue where it is pushed together (folds), green where
 * it lies relaxed, yellow to red where it is pulled tight. Ease: how far the cloth is from the body, red where it
 * presses on the body, through yellow, to green and blue where it hangs loose.
 */
class FitMap
{
    Q_DECLARE_TR_FUNCTIONS(Seamly3D::FitMap)

public:
    enum class Kind : int
    {
        None,
        Stretch,
        Ease
    };

    static QColor  color(Kind kind, float value);
    static QVector<QColor> colors(Kind kind, const QVector<float> &values);

    /** @brief A horizontal color bar for the legend, low values on the left. */
    static QImage  legend(Kind kind, int width, int height);
    static QString lowLabel(Kind kind);
    static QString highLabel(Kind kind);
    static QString name(Kind kind);
};

} // namespace Seamly3D

#endif // SEAMLY3D_FITMAP_H
