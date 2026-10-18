/******************************************************************************
 *   @file   dxfpatternconverter.h
 **  @author Seamly2D project
 **  @date   16 10, 2026
 **
 **  @brief  Converts the pieces of a DXF pattern into a parametric Seamly2D pattern (.val).
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

#ifndef SEAMLY3D_DXFPATTERNCONVERTER_H
#define SEAMLY3D_DXFPATTERNCONVERTER_H

#include <QCoreApplication>
#include <QLineF>
#include <QString>
#include <QStringList>
#include <QVector>

#include "dxfpattern.h"
#include "outlinefitter.h"

namespace Seamly3D
{

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief How a DXF pattern becomes a Seamly2D pattern.
 */
struct DxfImportOptions
{
    enum class SeamLine : int
    {
        Automatic,           ///< The file's sew line, with the allowance up to its cut line; else the cut line with
                             ///< the allowance included
        CutLineIncludesAllowance,
        CutLineAddAllowance  ///< The cut line is the seam line: add seamAllowance around it
    };

    QString       sourceUnit{QStringLiteral("cm")}; ///< The DXF file's units
    QString       targetUnit{QStringLiteral("cm")}; ///< The pattern's: "mm", "cm" or "inch"
    bool          fromPreviousPoint{true};          ///< Each point from the one before it; else from the piece's first
    SeamLine      seamLine{SeamLine::Automatic};
    double        seamAllowance{1.0};               ///< target units
    double        curveTolerance{0.05};             ///< centimeters
    double        cornerAngle{30};                  ///< degrees
    QVector<bool> include{};                        ///< per piece; empty: every piece
    QStringList   names{};                          ///< per piece; empty or blank: the piece's own name
    QVector<int>  quantities{};                     ///< per piece, how many are cut; empty or 0: as in the file
    QString       description{};                    ///< written in the pattern's description
    QString       patternName{};
};

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief A piece ready to write: placed on the page, in the pattern's units, Y down, clockwise.
 */
struct PreparedPiece
{
    int                 source{0};           ///< Index in DxfPattern::pieces
    QString             name{};
    int                 quantity{1};
    bool                onFold{false};
    FittedPath          outline{};           ///< The seam line (the main path)
    QPolygonF           cutLine{};
    QVector<FittedPath> internalLines{};
    QVector<QPointF>    drillHoles{};
    bool                hasGrainline{false};
    QLineF              grainline{};
    bool                seamAllowance{false};
    bool                seamAllowanceBuiltIn{false};
    double              width{0};            ///< seam allowance of the piece, target units
    QVector<double>     segmentWidths{};     ///< per outline point: allowance of the line or curve after it
};

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Writes a DXF pattern as a parametric Seamly2D pattern.
 *
 * Every piece gets a draft block of its own. Its first corner is a base point; every other point of its outline is
 * a point at a length and an angle from the point before it (or from the base point), so the pattern can be edited
 * like one drafted in Seamly2D. Curves are spline paths through their points, with handles that can be dragged.
 * The piece is made of these points and curves, with its notches, grain line, quantity, fold, internal lines (as
 * internal paths), drill holes (as anchor points) and seam allowance: from the sew line to the cut line when the file
 * has both, edge by edge.
 */
class DxfPatternConverter
{
    Q_DECLARE_TR_FUNCTIONS(Seamly3D::DxfPatternConverter)

public:
    static QVector<PreparedPiece> prepare(const DxfPattern &pattern, const DxfImportOptions &options);
    static QByteArray             write(const QVector<PreparedPiece> &pieces, const DxfImportOptions &options);

    /** @brief convert prepares and writes the pattern to @p fileName. */
    static bool convert(const DxfPattern &pattern, const DxfImportOptions &options, const QString &fileName,
                        QString *error = nullptr);

    /** @brief The pattern format version written (the converter of newer Seamly2D versions upgrades it). */
    static QString formatVersion();
};

} // namespace Seamly3D

#endif // SEAMLY3D_DXFPATTERNCONVERTER_H
