/******************************************************************************
 *   @file   workspacesetup.cpp
 **  @author Seamly2D project
 **  @date   16 10, 2026
 **
 **  @brief  What the 3D workspace keeps for a pattern, in a file next to it.
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

#include "workspacesetup.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace Seamly3D
{

namespace
{

const char *const formatName = "seamly3d-setup";

//---------------------------------------------------------------------------------------------------------------------
QString fitMapId(FitMap::Kind kind)
{
    switch (kind)
    {
        case FitMap::Kind::Stretch:
            return QStringLiteral("stretch");
        case FitMap::Kind::Ease:
            return QStringLiteral("ease");
        case FitMap::Kind::None:
        default:
            return QStringLiteral("none");
    }
}

//---------------------------------------------------------------------------------------------------------------------
FitMap::Kind fitMapFromId(const QString &id)
{
    if (id == QLatin1String("stretch"))
    {
        return FitMap::Kind::Stretch;
    }
    if (id == QLatin1String("ease"))
    {
        return FitMap::Kind::Ease;
    }
    return FitMap::Kind::None;
}

//---------------------------------------------------------------------------------------------------------------------
QJsonObject seamToJson(const SeamSpec &seam)
{
    QJsonObject object;
    object.insert(QStringLiteral("pieceA"), static_cast<qint64>(seam.pieceA));
    object.insert(QStringLiteral("copyA"), seam.copyA);
    object.insert(QStringLiteral("edgeA"), seam.edgeA);
    object.insert(QStringLiteral("pieceB"), static_cast<qint64>(seam.pieceB));
    object.insert(QStringLiteral("copyB"), seam.copyB);
    object.insert(QStringLiteral("edgeB"), seam.edgeB);
    object.insert(QStringLiteral("reversed"), seam.reversed);
    object.insert(QStringLiteral("startA"), seam.startA);
    object.insert(QStringLiteral("endA"), seam.endA);
    return object;
}

//---------------------------------------------------------------------------------------------------------------------
SeamSpec seamFromJson(const QJsonObject &object)
{
    SeamSpec seam;
    seam.pieceA = static_cast<quint32>(object.value(QStringLiteral("pieceA")).toDouble());
    seam.copyA = object.value(QStringLiteral("copyA")).toInt();
    seam.edgeA = object.value(QStringLiteral("edgeA")).toInt();
    seam.pieceB = static_cast<quint32>(object.value(QStringLiteral("pieceB")).toDouble());
    seam.copyB = object.value(QStringLiteral("copyB")).toInt();
    seam.edgeB = object.value(QStringLiteral("edgeB")).toInt();
    seam.reversed = object.value(QStringLiteral("reversed")).toBool();
    seam.startA = qBound(0.0, object.value(QStringLiteral("startA")).toDouble(0.0), 1.0);
    seam.endA = qBound(seam.startA, object.value(QStringLiteral("endA")).toDouble(1.0), 1.0);
    seam.added = true;
    return seam;
}

} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
QString SeamSpec::key() const
{
    return QStringLiteral("%1:%2:%3-%4:%5:%6@%7")
        .arg(pieceA).arg(copyA).arg(edgeA).arg(pieceB).arg(copyB).arg(edgeB).arg(startA, 0, 'f', 3);
}

//---------------------------------------------------------------------------------------------------------------------
QString WorkspaceSetup::fileFor(const QString &patternFile)
{
    if (patternFile.isEmpty())
    {
        return QString();
    }
    const QFileInfo info(patternFile);
    return info.dir().filePath(info.completeBaseName() + QStringLiteral(".seamly3d"));
}

//---------------------------------------------------------------------------------------------------------------------
QByteArray WorkspaceSetup::toJson() const
{
    QJsonObject root;
    root.insert(QStringLiteral("format"), QLatin1String(formatName));
    root.insert(QStringLiteral("version"), version);
    if (!fabric.isEmpty())
    {
        root.insert(QStringLiteral("fabric"), fabric);
    }
    root.insert(QStringLiteral("fitMap"), fitMapId(fitMap));

    QJsonArray pieceArray;
    for (const PieceSetup &piece : pieces)
    {
        QJsonObject object;
        object.insert(QStringLiteral("id"), static_cast<qint64>(piece.id));
        object.insert(QStringLiteral("name"), piece.name);
        object.insert(QStringLiteral("placement"), PieceArrangement::placementId(piece.placement));
        object.insert(QStringLiteral("pair"), piece.pair);
        object.insert(QStringLiteral("flip"), piece.flip);
        if (!piece.adjustment.isNull())
        {
            object.insert(QStringLiteral("lift"), static_cast<double>(piece.adjustment.lift));
            object.insert(QStringLiteral("around"), static_cast<double>(piece.adjustment.around));
            object.insert(QStringLiteral("turn"), static_cast<double>(piece.adjustment.turn));
        }
        pieceArray.append(object);
    }
    root.insert(QStringLiteral("pieces"), pieceArray);

    QJsonObject seams;
    seams.insert(QStringLiteral("off"), QJsonArray::fromStringList(switchedOff));
    QJsonArray addedArray;
    for (const SeamSpec &seam : added)
    {
        addedArray.append(seamToJson(seam));
    }
    seams.insert(QStringLiteral("added"), addedArray);
    root.insert(QStringLiteral("seams"), seams);

    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

//---------------------------------------------------------------------------------------------------------------------
bool WorkspaceSetup::fromJson(const QByteArray &json, WorkspaceSetup *setup, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        if (error != nullptr)
        {
            *error = tr("Not a 3D setup: %1").arg(parseError.errorString());
        }
        return false;
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("format")).toString() != QLatin1String(formatName))
    {
        if (error != nullptr)
        {
            *error = tr("Not a 3D setup.");
        }
        return false;
    }

    WorkspaceSetup result;
    result.fabric = root.value(QStringLiteral("fabric")).toString();
    result.fitMap = fitMapFromId(root.value(QStringLiteral("fitMap")).toString());
    for (const QJsonValue &value : root.value(QStringLiteral("pieces")).toArray())
    {
        const QJsonObject object = value.toObject();
        PieceSetup piece;
        piece.id = static_cast<quint32>(object.value(QStringLiteral("id")).toDouble());
        piece.name = object.value(QStringLiteral("name")).toString();
        bool known = false;
        piece.placement = PieceArrangement::placementFromId(object.value(QStringLiteral("placement")).toString(),
                                                            &known);
        if (!known)
        {
            continue; // a placement from a newer version: the piece keeps its guess
        }
        piece.pair = object.value(QStringLiteral("pair")).toBool();
        piece.flip = object.value(QStringLiteral("flip")).toBool();
        const auto number = [&object](const char *key, double limit)
        { return static_cast<float>(qBound(-limit, object.value(QLatin1String(key)).toDouble(), limit)); };
        piece.adjustment.lift = number("lift", 150.0);
        piece.adjustment.around = number("around", 180.0);
        piece.adjustment.turn = number("turn", 180.0);
        result.pieces.append(piece);
    }
    const QJsonObject seams = root.value(QStringLiteral("seams")).toObject();
    for (const QJsonValue &value : seams.value(QStringLiteral("off")).toArray())
    {
        result.switchedOff.append(value.toString());
    }
    for (const QJsonValue &value : seams.value(QStringLiteral("added")).toArray())
    {
        result.added.append(seamFromJson(value.toObject()));
    }
    *setup = result;
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
bool WorkspaceSetup::save(const QString &file, QString *error) const
{
    // Written in full or not at all: a crash halfway leaves the previous setup.
    QSaveFile out(file);
    if (!out.open(QIODevice::WriteOnly) || out.write(toJson()) < 0 || !out.commit())
    {
        if (error != nullptr)
        {
            *error = out.errorString();
        }
        return false;
    }
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
bool WorkspaceSetup::load(const QString &file, WorkspaceSetup *setup, QString *error)
{
    QFile in(file);
    if (!in.open(QIODevice::ReadOnly))
    {
        if (error != nullptr)
        {
            *error = in.errorString();
        }
        return false;
    }
    return fromJson(in.readAll(), setup, error);
}

} // namespace Seamly3D
