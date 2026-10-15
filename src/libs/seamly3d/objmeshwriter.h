/******************************************************************************
 *   @file   objmeshwriter.h
 **  @author Seamly2D project
 **  @date   15 10, 2026
 **
 **  @brief  Wavefront OBJ writer for meshes.
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

#ifndef SEAMLY3D_OBJMESHWRITER_H
#define SEAMLY3D_OBJMESHWRITER_H

#include <QCoreApplication>
#include <QString>

#include "mesh.h"

class QIODevice;

namespace Seamly3D
{

/**
 * @brief Writes a mesh as Wavefront OBJ (positions, normals, triangles) so bodies can be used in other 3D software.
 */
class ObjMeshWriter
{
    Q_DECLARE_TR_FUNCTIONS(Seamly3D::ObjMeshWriter)

public:
    ObjMeshWriter() = default;

    bool    writeFile(const QString &fileName, const Mesh &mesh);
    bool    write(QIODevice *device, const Mesh &mesh);

    QString errorString() const;

private:
    QString m_errorString{};
};

} // namespace Seamly3D

#endif // SEAMLY3D_OBJMESHWRITER_H
