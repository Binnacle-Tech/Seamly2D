/******************************************************************************
 *   @file   workspacesetup.h
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

#ifndef SEAMLY3D_WORKSPACESETUP_H
#define SEAMLY3D_WORKSPACESETUP_H

#include <QByteArray>
#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QVector>

#include "fitmap.h"
#include "piecearrangement.h"

namespace Seamly3D
{

/** @brief A seam between two edges, named by piece, copy (0, or 1 for the mirrored copy) and edge number. */
struct SeamSpec
{
    quint32 pieceA{0};
    int     copyA{0};
    int     edgeA{0};
    quint32 pieceB{0};
    int     copyB{0};
    int     edgeB{0};
    bool    reversed{false};
    double  startA{0.0};
    double  endA{1.0};
    bool    added{false}; ///< added by hand; otherwise found automatically

    QString key() const;
};

/** @brief Where one piece goes around the body. The name finds the piece again if its id changed. */
struct PieceSetup
{
    quint32        id{0};
    QString        name{};
    PiecePlacement placement{PiecePlacement::Aside};
    bool           pair{false};
    bool           flip{false};
};

/**
 * @brief What the 3D workspace keeps for a pattern: where its pieces go, the seams switched off or added by hand,
 * the fabric and the fit map.
 *
 * It is saved next to the pattern, in a JSON file of its own (shirt.val, shirt.seamly3d), so the pattern file and its
 * schema stay as they are.
 */
class WorkspaceSetup
{
    Q_DECLARE_TR_FUNCTIONS(Seamly3D::WorkspaceSetup)

public:
    QVector<PieceSetup> pieces{};
    QStringList         switchedOff{}; ///< SeamSpec::key() of the seams found but left open
    QVector<SeamSpec>   added{};
    QString             fabric{};
    FitMap::Kind        fitMap{FitMap::Kind::None};

    /** @brief fileFor returns the setup file of a pattern file; empty for a pattern not saved yet. */
    static QString fileFor(const QString &patternFile);

    QByteArray toJson() const;
    /** @brief fromJson reads a setup; what it does not know (from a newer version) is left out. */
    static bool fromJson(const QByteArray &json, WorkspaceSetup *setup, QString *error = nullptr);

    bool save(const QString &file, QString *error = nullptr) const;
    static bool load(const QString &file, WorkspaceSetup *setup, QString *error = nullptr);

    static constexpr int version = 1;
};

} // namespace Seamly3D

#endif // SEAMLY3D_WORKSPACESETUP_H
