/******************************************************************************
 *   @file   workspace3dintegration.cpp
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

#include "workspace3dintegration.h"

#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>

#include "measurementguide.h"
#include "workspace3dwindow.h"

namespace
{
// Object names of the host's View and Help menus (see mainwindow.ui). Looked up at runtime so the 2D UI file stays
// untouched.
const QString hostViewMenuName = QStringLiteral("view_Menu");
const QString hostHelpMenuName = QStringLiteral("help_Menu");
} // anonymous namespace

namespace Seamly3D
{

//---------------------------------------------------------------------------------------------------------------------
Workspace3DIntegration *Workspace3DIntegration::install(QMainWindow *host)
{
    if (host == nullptr)
    {
        return nullptr;
    }

    // Installing twice is harmless: reuse the existing integration.
    if (auto *existing = host->findChild<Workspace3DIntegration *>(QString(), Qt::FindDirectChildrenOnly))
    {
        return existing;
    }
    return new Workspace3DIntegration(host);
}

//---------------------------------------------------------------------------------------------------------------------
Workspace3DIntegration::Workspace3DIntegration(QMainWindow *host)
    : QObject(host)
    , m_host(host)
    , m_action(new QAction(tr("3D Workspace"), this))
{
    m_action->setObjectName(QStringLiteral("seamly3dWorkspace_Action"));
    m_action->setToolTip(tr("Open the 3D workspace"));
    connect(m_action, &QAction::triggered, this, &Workspace3DIntegration::showWorkspace);

    if (auto *viewMenu = host->findChild<QMenu *>(hostViewMenuName))
    {
        viewMenu->addSeparator();
        viewMenu->addAction(m_action);
    }
    else
    {
        host->menuBar()->addAction(m_action);
    }

    // Help > How to Measure: where on the body to take each measurement.
    if (auto *helpMenu = host->findChild<QMenu *>(hostHelpMenuName))
    {
        auto *guideAction = new QAction(tr("How to Measure (Body)"), this);
        guideAction->setObjectName(QStringLiteral("seamly3dMeasuringGuide_Action"));
        guideAction->setToolTip(tr("Where on the body to take each measurement"));
        connect(guideAction, &QAction::triggered, this,
                [host]()
                {
                    auto *dialog = new MeasurementGuideDialog(host);
                    dialog->setAttribute(Qt::WA_DeleteOnClose);
                    dialog->show();
                });
        helpMenu->insertAction(helpMenu->actions().value(0), guideAction);
        helpMenu->insertSeparator(helpMenu->actions().value(1));
    }
}

//---------------------------------------------------------------------------------------------------------------------
QAction *Workspace3DIntegration::action() const
{
    return m_action;
}

//---------------------------------------------------------------------------------------------------------------------
Workspace3DWindow *Workspace3DIntegration::window() const
{
    return m_window;
}

//---------------------------------------------------------------------------------------------------------------------
void Workspace3DIntegration::showWorkspace()
{
    if (m_window.isNull())
    {
        // Parented to the host so it closes with the application; it is still its own top level window.
        m_window = new Workspace3DWindow(m_host);
    }

    m_window->show();
    m_window->raise();
    m_window->activateWindow();
}

} // namespace Seamly3D
