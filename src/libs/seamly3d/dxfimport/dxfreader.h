/******************************************************************************
 *   @file   dxfreader.h
 **  @author Seamly2D project
 **  @date   16 10, 2026
 **
 **  @brief  Reads the drawing in a DXF file: blocks, inserts, lines, polylines, curves and text.
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

#ifndef SEAMLY3D_DXFREADER_H
#define SEAMLY3D_DXFREADER_H

#include <QCoreApplication>
#include <QHash>
#include <QLineF>
#include <QPointF>
#include <QPolygonF>
#include <QString>
#include <QVector>

class QIODevice;

namespace Seamly3D
{

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief One drawing entity of a DXF file, in the file's own units (Y up).
 */
struct DxfEntity
{
    QString            type{};       ///< LINE, POLYLINE, LWPOLYLINE, POINT, TEXT, MTEXT, CIRCLE, ARC, ELLIPSE, SPLINE,
                                     ///< INSERT
    QString            layer{};
    QVector<QPointF>   points{};     ///< Vertices; the two ends of a LINE; the center of a CIRCLE, ARC or ELLIPSE;
                                     ///< the position of a POINT, TEXT or INSERT; the control points of a SPLINE
    QVector<double>    bulges{};     ///< Polylines: the arc from each vertex to the next (0 is straight)
    bool               closed{false};
    QString            text{};       ///< TEXT and MTEXT; the block name of an INSERT
    double             radius{0};    ///< CIRCLE and ARC
    double             startAngle{0};///< ARC (degrees) and ELLIPSE (parameter, radians)
    double             endAngle{360};
    QPointF            majorAxis{};  ///< ELLIPSE: the end of the major axis, from the center
    double             ratio{1};     ///< ELLIPSE: minor to major axis
    double             rotation{0};  ///< INSERT and TEXT, degrees
    QPointF            scale{1, 1};  ///< INSERT
    int                degree{3};    ///< SPLINE
    QVector<double>    knots{};      ///< SPLINE
    QVector<double>    weights{};    ///< SPLINE, empty when all are 1
    QVector<QPointF>   fitPoints{};  ///< SPLINE

    /** @brief isCurve is true for entities with a length: lines, polylines, arcs, circles, ellipses, splines. */
    bool      isCurve() const;
    /** @brief flatten returns the entity as a polyline, curves within @p tolerance of the true shape. */
    QPolygonF flatten(double tolerance) const;
};

//---------------------------------------------------------------------------------------------------------------------
struct DxfBlock
{
    QString            name{};
    QPointF            base{};
    QVector<DxfEntity> entities{};
};

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief The drawing of a DXF file: header variables, blocks, and the entities of the drawing itself.
 */
struct DxfDocument
{
    QHash<QString, QString> header{}; ///< e.g. $INSUNITS -> "1"
    QVector<DxfBlock>       blocks{};
    QVector<DxfEntity>      entities{};

    const DxfBlock *block(const QString &name) const;
};

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Reads ASCII DXF files, any version (R12 to 2018).
 *
 * Only the geometry and text a pattern needs are kept: LINE, POLYLINE (with VERTEX), LWPOLYLINE, POINT, TEXT, MTEXT,
 * CIRCLE, ARC, ELLIPSE, SPLINE and INSERT, in the ENTITIES section and in BLOCKS. Binary DXF is not supported.
 */
class DxfReader
{
    Q_DECLARE_TR_FUNCTIONS(Seamly3D::DxfReader)

public:
    bool read(const QString &fileName, DxfDocument &document);
    bool read(QIODevice *device, DxfDocument &document);
    bool parse(const QByteArray &data, DxfDocument &document);

    QString errorString() const;

private:
    QString m_error{};
};

} // namespace Seamly3D

#endif // SEAMLY3D_DXFREADER_H
