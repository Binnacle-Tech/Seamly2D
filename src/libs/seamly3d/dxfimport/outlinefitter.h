/******************************************************************************
 *   @file   outlinefitter.h
 **  @author Seamly2D project
 **  @date   16 10, 2026
 **
 **  @brief  Turns a densely sampled outline into corners, straight lines and a few smooth curves.
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

#ifndef SEAMLY3D_OUTLINEFITTER_H
#define SEAMLY3D_OUTLINEFITTER_H

#include <QPainterPath>
#include <QPointF>
#include <QPolygonF>
#include <QVector>

namespace Seamly3D
{

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief A point of a fitted outline: where it is, and the line or curve to the next point.
 */
struct FittedPoint
{
    QPointF position{};
    bool    corner{false};    ///< The outline turns sharply here: curves on either side are independent
    bool    notch{false};
    bool    lineAfter{true};  ///< Straight to the next point; otherwise a cubic curve
    QPointF handleIn{};       ///< The curve from the previous point: its second control point, from here
    QPointF handleOut{};      ///< The curve to the next point: its first control point, from here
};

//---------------------------------------------------------------------------------------------------------------------
struct FittedPath
{
    QVector<FittedPoint> points{};
    bool                 closed{true};
    double               maxDeviation{0}; ///< Farthest the result strays from the original outline

    QPainterPath painterPath() const;
    /** @brief The fitted outline as a polyline, curves sampled every @p step or finer. */
    QPolygonF    polygon(int samplesPerCurve = 16) const;
    int          curveCount() const;      ///< Curved segments
};

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Fits an outline with corners, straight lines and smooth curves through as few points as needed.
 *
 * Corners are where the outline turns more than the corner angle, and the given corners (AAMA turn points). Long
 * edges of the outline stay straight lines. Notches become points of the outline. Everything else is fitted with
 * smooth cubic curves (spline paths) through the given curve points (AAMA curve points), adding points until the
 * curves are within the tolerance of the outline everywhere.
 */
class OutlineFitter
{
public:
    struct Settings
    {
        double tolerance{0.05};      ///< Largest distance between the curves and the outline
        double cornerAngle{30};      ///< degrees
        double straightLength{2.0};  ///< Edges at least this long (and much longer than their neighbours) are lines
    };

    static FittedPath fit(const QPolygonF &outline, bool closed, const Settings &settings,
                          const QVector<QPointF> &corners = QVector<QPointF>(),
                          const QVector<QPointF> &notches = QVector<QPointF>(),
                          const QVector<QPointF> &curvePoints = QVector<QPointF>());
};

} // namespace Seamly3D

#endif // SEAMLY3D_OUTLINEFITTER_H
