/******************************************************************************
 *   @file   svgreader.h
 **  @author Seamly2D project
 **  @date   16 10, 2026
 **
 **  @brief  Reads the drawing of an SVG file into the same form as a DXF drawing.
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

#ifndef SEAMLY3D_SVGREADER_H
#define SEAMLY3D_SVGREADER_H

#include <QCoreApplication>
#include <QString>

#include "dxfreader.h"

class QIODevice;

namespace Seamly3D
{

/**
 * @brief Reads SVG drawings (patterns from FreeSewing, Inkscape, Illustrator...) as a DXF document in millimeters.
 *
 * Paths (all commands, curves and arcs flattened finely), lines, polylines, polygons, rectangles, circles and
 * ellipses become polylines; text becomes text. Group and element transforms, <use> references and nested <svg>
 * are followed; hidden elements, <defs>, clip paths, masks and patterns are not drawn. The size of the drawing in
 * real units (its width and height, with its viewBox) sets the scale; without units, 96 pixels per inch. Y points up
 * in the result, as in DXF. The layer of each entity is the element's class, else its id.
 */
class SvgReader
{
    Q_DECLARE_TR_FUNCTIONS(Seamly3D::SvgReader)

public:
    bool read(const QString &fileName, DxfDocument &document);
    bool read(QIODevice *device, DxfDocument &document);
    bool parse(const QByteArray &data, DxfDocument &document);

    QString errorString() const;

    /** @brief isSvg tells SVG from DXF by the content of a file. */
    static bool isSvg(const QByteArray &start);

private:
    QString m_error{};
};

} // namespace Seamly3D

#endif // SEAMLY3D_SVGREADER_H
