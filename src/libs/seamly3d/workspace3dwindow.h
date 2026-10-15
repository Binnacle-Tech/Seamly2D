/******************************************************************************
 *   @file   workspace3dwindow.h
 **  @author Seamly2D project
 **  @date   15 10, 2026
 **
 **  @brief  Top level window of the 3D workspace.
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

#ifndef SEAMLY3D_WORKSPACE3DWINDOW_H
#define SEAMLY3D_WORKSPACE3DWINDOW_H

#include <QMainWindow>
#include <QString>

class QAction;
class QLabel;

namespace Seamly3D
{

class Viewport3D;

/**
 * @brief Top level window of the 3D workspace.
 *
 * Hosts the 3D viewport and its tools. It is independent of the 2D main window so the 3D workspace can grow
 * (avatars, measurement mapping, garment draping) without touching the 2D code.
 */
class Workspace3DWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit Workspace3DWindow(QWidget *parent = nullptr);
    ~Workspace3DWindow() override = default;

    Viewport3D *viewport() const;

    bool        loadBodyModel(const QString &fileName);

public slots:
    void        importBodyModel();
    void        clearBodyModel();

private:
    Q_DISABLE_COPY_MOVE(Workspace3DWindow)

    void        createActions();
    void        updateStatus();

    Viewport3D *m_viewport;
    QLabel     *m_statusLabel;
    QAction    *m_clearAction{nullptr};
    QString     m_bodyModelPath{};
    QString     m_lastDirectory{};
};

} // namespace Seamly3D

#endif // SEAMLY3D_WORKSPACE3DWINDOW_H
