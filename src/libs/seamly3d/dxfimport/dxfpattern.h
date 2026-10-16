/******************************************************************************
 *   @file   dxfpattern.h
 **  @author Seamly2D project
 **  @date   16 10, 2026
 **
 **  @brief  Pattern pieces found in a DXF drawing: AAMA/ASTM layers, or closed outlines of any DXF.
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

#ifndef SEAMLY3D_DXFPATTERN_H
#define SEAMLY3D_DXFPATTERN_H

#include <QCoreApplication>
#include <QLineF>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

#include "dxfreader.h"

namespace Seamly3D
{

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief One pattern piece read from a DXF file, in the file's units, Y up (as in the file).
 */
struct DxfPiece
{
    QString            name{};
    QString            blockName{};
    int                quantity{1};
    QString            size{};
    QString            category{};
    QString            material{};
    QString            annotation{};
    QPolygonF          cutLine{};       ///< Closed outline the piece is cut on
    QPolygonF          sewLine{};       ///< Closed seam line, when the file has one (AAMA layer 14)
    QVector<QPointF>   turnPoints{};    ///< Corners (AAMA layer 2)
    QVector<QPointF>   curvePoints{};   ///< Points a curve passes through (AAMA layer 3)
    QVector<QLineF>    notches{};       ///< p1 on the cut line (AAMA layer 4)
    QVector<QPointF>   drillHoles{};    ///< AAMA layer 13
    QVector<QPolygonF> internalLines{}; ///< AAMA layers 8 and 11; closed ones end where they start
    bool               hasGrainline{false};
    QLineF             grainline{};     ///< AAMA layer 7
    bool               hasMirrorLine{false};
    QLineF             mirrorLine{};    ///< The fold, AAMA layer 6

    /** @brief The line the piece is sewn on: the sew line when there is one, otherwise the cut line. */
    const QPolygonF &outline() const;
    QRectF bounds() const;
};

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief The pattern in a DXF file.
 */
struct DxfPattern
{
    bool              astm{false};     ///< AAMA or ASTM D6673 pattern exchange file
    QString           unit{};          ///< The file's units: "mm", "cm", "inch", "m" or "ft"
    QString           unitSource{};    ///< How the units were found, for the user
    QString           styleName{};
    QVector<DxfPiece> pieces{};
    QStringList       warnings{};

    /** @brief toCentimeters is how many centimeters one unit of @p unit is. */
    static double toCentimeters(const QString &unit);
};

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Finds the pattern pieces in a DXF drawing.
 *
 * AAMA / ASTM D6673 files (what most pattern CAD systems export: Gerber, Lectra, Optitex, CLO, Seamly2D...) have a
 * block per piece: the cut line on layer 1, turn points on 2, curve points on 3, notches on 4, the mirror (fold) line
 * on 6, the grain line on 7, internal lines on 8 and 11, drill holes on 13, the sew line on 14, and text such as
 * "Piece Name: Front" and "Quantity: 2". Any other DXF: every closed outline that is not inside another is a piece;
 * an outline inside it running parallel to it is its sew line, others are internal lines.
 */
class DxfPatternReader
{
    Q_DECLARE_TR_FUNCTIONS(Seamly3D::DxfPatternReader)

public:
    static DxfPattern extract(const DxfDocument &document);

    /** @brief How finely curves (arcs, splines) are turned into points, in the file's units. */
    static double flatteningTolerance(const QString &unit);
};

} // namespace Seamly3D

#endif // SEAMLY3D_DXFPATTERN_H
