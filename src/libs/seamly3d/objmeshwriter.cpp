/******************************************************************************
 *   @file   objmeshwriter.cpp
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

#include "objmeshwriter.h"

#include <QIODevice>
#include <QSaveFile>
#include <QTextStream>

namespace Seamly3D
{

//---------------------------------------------------------------------------------------------------------------------
bool ObjMeshWriter::writeFile(const QString &fileName, const Mesh &mesh)
{
    QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        m_errorString = tr("Cannot write file %1: %2").arg(fileName, file.errorString());
        return false;
    }

    if (!write(&file, mesh))
    {
        file.cancelWriting();
        return false;
    }

    if (!file.commit())
    {
        m_errorString = tr("Cannot write file %1: %2").arg(fileName, file.errorString());
        return false;
    }
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
bool ObjMeshWriter::write(QIODevice *device, const Mesh &mesh)
{
    m_errorString.clear();
    if (device == nullptr || !device->isWritable())
    {
        m_errorString = tr("Device is not writable.");
        return false;
    }
    if (mesh.isEmpty() || !mesh.isValid())
    {
        m_errorString = tr("There is no valid mesh to write.");
        return false;
    }

    const bool hasNormals = mesh.normals.size() == mesh.positions.size();

    QTextStream out(device);
    out.setRealNumberNotation(QTextStream::SmartNotation);
    out.setRealNumberPrecision(7);
    out << "# Seamly2D 3D workspace\n";

    for (const QVector3D &p : mesh.positions)
    {
        out << "v " << p.x() << ' ' << p.y() << ' ' << p.z() << '\n';
    }
    if (hasNormals)
    {
        for (const QVector3D &n : mesh.normals)
        {
            out << "vn " << n.x() << ' ' << n.y() << ' ' << n.z() << '\n';
        }
    }
    for (int i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        out << 'f';
        for (int k = 0; k < 3; ++k)
        {
            const quint32 index = mesh.indices.at(i + k) + 1;
            out << ' ' << index;
            if (hasNormals)
            {
                out << "//" << index;
            }
        }
        out << '\n';
    }

    out.flush();
    if (out.status() != QTextStream::Ok)
    {
        m_errorString = tr("Failed to write the mesh.");
        return false;
    }
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
QString ObjMeshWriter::errorString() const
{
    return m_errorString;
}

} // namespace Seamly3D
