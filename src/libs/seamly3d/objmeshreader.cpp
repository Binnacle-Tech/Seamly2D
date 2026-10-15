/******************************************************************************
 *   @file   objmeshreader.cpp
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

#include "objmeshreader.h"

#include <QFile>
#include <QHash>
#include <QIODevice>
#include <QStringList>

namespace
{

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief resolveIndex converts a 1-based (or negative, relative) OBJ index into a 0-based index.
 * @return -1 if the index is out of range or not a number.
 */
int resolveIndex(const QString &token, int count)
{
    bool ok = false;
    const int value = token.toInt(&ok);
    if (!ok || value == 0)
    {
        return -1;
    }

    const int index = value > 0 ? value - 1 : count + value;
    return (index >= 0 && index < count) ? index : -1;
}

//---------------------------------------------------------------------------------------------------------------------
bool parseVector(const QStringList &parts, QVector3D &vector)
{
    if (parts.size() < 4)
    {
        return false;
    }

    bool okX = false;
    bool okY = false;
    bool okZ = false;
    vector = QVector3D(parts.at(1).toFloat(&okX), parts.at(2).toFloat(&okY), parts.at(3).toFloat(&okZ));
    return okX && okY && okZ;
}

//---------------------------------------------------------------------------------------------------------------------
struct CornerKey
{
    int position{-1};
    int normal{-1};
};

bool operator==(const CornerKey &lhs, const CornerKey &rhs)
{
    return lhs.position == rhs.position && lhs.normal == rhs.normal;
}

size_t qHash(const CornerKey &key, size_t seed = 0)
{
    return ::qHash(qMakePair(key.position, key.normal), seed);
}

} // anonymous namespace

namespace Seamly3D
{

//---------------------------------------------------------------------------------------------------------------------
bool ObjMeshReader::readFile(const QString &fileName, Mesh &mesh)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        m_errorString = tr("Cannot open file %1: %2").arg(fileName, file.errorString());
        return false;
    }
    return read(&file, mesh);
}

//---------------------------------------------------------------------------------------------------------------------
bool ObjMeshReader::read(QIODevice *device, Mesh &mesh)
{
    m_errorString.clear();
    mesh.clear();

    if (device == nullptr || !device->isReadable())
    {
        m_errorString = tr("Device is not readable.");
        return false;
    }

    QVector<QVector3D> filePositions;
    QVector<QVector3D> fileNormals;

    // OBJ indexes positions and normals separately; GPU meshes need one index per unique corner.
    QHash<CornerKey, quint32> cornerIndex;
    bool allCornersHaveNormals = true;

    int lineNumber = 0;
    while (!device->atEnd())
    {
        ++lineNumber;
        const QString line = QString::fromUtf8(device->readLine()).simplified();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
        {
            continue;
        }

        const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        const QString &keyword = parts.constFirst();

        if (keyword == QLatin1String("v"))
        {
            QVector3D position;
            if (!parseVector(parts, position))
            {
                m_errorString = tr("Invalid vertex at line %1.").arg(lineNumber);
                return false;
            }
            filePositions.append(position);
        }
        else if (keyword == QLatin1String("vn"))
        {
            QVector3D normal;
            if (!parseVector(parts, normal))
            {
                m_errorString = tr("Invalid vertex normal at line %1.").arg(lineNumber);
                return false;
            }
            fileNormals.append(normal.normalized());
        }
        else if (keyword == QLatin1String("f"))
        {
            if (parts.size() < 4)
            {
                m_errorString = tr("Face with less than three vertices at line %1.").arg(lineNumber);
                return false;
            }

            QVector<quint32> polygon;
            polygon.reserve(parts.size() - 1);
            for (int i = 1; i < parts.size(); ++i)
            {
                const QStringList refs = parts.at(i).split(QLatin1Char('/'));

                CornerKey key;
                key.position = resolveIndex(refs.at(0), static_cast<int>(filePositions.size()));
                if (key.position < 0)
                {
                    m_errorString = tr("Invalid vertex index at line %1.").arg(lineNumber);
                    return false;
                }

                if (refs.size() >= 3 && !refs.at(2).isEmpty())
                {
                    key.normal = resolveIndex(refs.at(2), static_cast<int>(fileNormals.size()));
                    if (key.normal < 0)
                    {
                        m_errorString = tr("Invalid normal index at line %1.").arg(lineNumber);
                        return false;
                    }
                }
                else
                {
                    allCornersHaveNormals = false;
                }

                auto it = cornerIndex.constFind(key);
                if (it == cornerIndex.constEnd())
                {
                    const auto newIndex = static_cast<quint32>(mesh.positions.size());
                    mesh.positions.append(filePositions.at(key.position));
                    mesh.normals.append(key.normal >= 0 ? fileNormals.at(key.normal) : QVector3D());
                    it = cornerIndex.insert(key, newIndex);
                }
                polygon.append(it.value());
            }

            for (int i = 1; i + 1 < polygon.size(); ++i)
            {
                mesh.indices << polygon.at(0) << polygon.at(i) << polygon.at(i + 1);
            }
        }
        // Other statements (vt, o, g, s, usemtl, mtllib, l, p...) are not needed yet and are skipped.
    }

    if (mesh.isEmpty())
    {
        m_errorString = tr("The file does not contain any faces.");
        mesh.clear();
        return false;
    }

    if (!allCornersHaveNormals)
    {
        mesh.computeNormals();
    }

    return true;
}

//---------------------------------------------------------------------------------------------------------------------
QString ObjMeshReader::errorString() const
{
    return m_errorString;
}

} // namespace Seamly3D
