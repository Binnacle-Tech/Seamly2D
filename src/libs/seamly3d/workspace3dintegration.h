/******************************************************************************
 *   @file   workspace3dintegration.h
 **  @author Seamly2D project
 **  @date   15 10, 2026
 **
 **  @brief  Hooks the 3D workspace into the 2D main window.
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

#ifndef SEAMLY3D_WORKSPACE3DINTEGRATION_H
#define SEAMLY3D_WORKSPACE3DINTEGRATION_H

#include <QObject>
#include <QPointer>

class QAction;
class QDockWidget;
class QMainWindow;

namespace Seamly3D
{

class Workspace3DWindow;

/**
 * @brief The single integration point between the 2D application and the 3D workspace.
 *
 * Call Workspace3DIntegration::install() once from the host main window. It adds a "3D Workspace" toggle to the host's
 * View menu (falling back to the menu bar) and "How to Measure (Body)" to its Help menu. The 3D workspace opens as a
 * dock of the main window, next to the 2D pattern (it can be floated out); it is created on first use. No other 2D code
 * needs to know about the 3D workspace.
 */
class Workspace3DIntegration : public QObject
{
    Q_OBJECT

public:
    static Workspace3DIntegration *install(QMainWindow *host);

    QAction           *action() const;
    Workspace3DWindow *window() const;

public slots:
    void showWorkspace();
    void setWorkspaceVisible(bool visible);

private:
    explicit Workspace3DIntegration(QMainWindow *host);
    Q_DISABLE_COPY_MOVE(Workspace3DIntegration)

    QMainWindow                 *m_host;
    QAction                     *m_action;
    QPointer<Workspace3DWindow>  m_window{};
    QPointer<QDockWidget>        m_dock{};
};

} // namespace Seamly3D

#endif // SEAMLY3D_WORKSPACE3DINTEGRATION_H
