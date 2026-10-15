/******************************************************************************
 *   @file   objmeshreader.h
 **  @author Seamly2D project
 **  @date   15 10, 2026
 **
 **  @brief  Wavefront OBJ reader for 3D body models (avatars).
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

#ifndef SEAMLY3D_OBJMESHREADER_H
#define SEAMLY3D_OBJMESHREADER_H

#include <QCoreApplication>
#include <QString>

#include "mesh.h"

class QIODevice;

namespace Seamly3D
{

/**
 * @brief Minimal Wavefront OBJ reader for body models (avatars).
 *
 * Supports the subset needed for scanned or modeled bodies: vertices (v), vertex normals (vn) and polygonal faces (f)
 * in any of the v, v/vt, v//vn and v/vt/vn forms, including negative (relative) indices. Polygons with more than three
 * corners are fan triangulated. Groups, materials and texture coordinates are ignored for now.
 *
 * All objects in the file are merged into one mesh. When the file does not provide a normal for every corner,
 * smooth normals are calculated.
 */
class ObjMeshReader
{
    Q_DECLARE_TR_FUNCTIONS(Seamly3D::ObjMeshReader)

public:
    ObjMeshReader() = default;

    bool    readFile(const QString &fileName, Mesh &mesh);
    bool    read(QIODevice *device, Mesh &mesh);

    QString errorString() const;

private:
    QString m_errorString{};
};

} // namespace Seamly3D

#endif // SEAMLY3D_OBJMESHREADER_H
