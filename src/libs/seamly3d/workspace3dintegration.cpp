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
#include <QDockWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QSignalBlocker>

#include "measurementguide.h"
#include "patternsource.h"
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
    m_action->setToolTip(tr("Show or hide the 3D workspace"));
    m_action->setCheckable(true);
    connect(m_action, &QAction::toggled, this, &Workspace3DIntegration::setWorkspaceVisible);

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
    m_action->setChecked(true);
    setWorkspaceVisible(true);
}

//---------------------------------------------------------------------------------------------------------------------
void Workspace3DIntegration::setWorkspaceVisible(bool visible)
{
    if (m_dock.isNull())
    {
        if (!visible)
        {
            return;
        }

        // A dock of the main window, beside the 2D pattern; it can be floated out to a screen of its own.
        m_dock = new QDockWidget(tr("3D Workspace"), m_host);
        m_dock->setObjectName(QStringLiteral("seamly3dWorkspace_Dock"));
        m_window = new Workspace3DWindow(m_dock);
        m_window->setWindowFlags(Qt::Widget);
        m_window->setPatternProvider(std::make_shared<OpenPatternProvider>());
        m_dock->setWidget(m_window);
        m_host->addDockWidget(Qt::RightDockWidgetArea, m_dock);
        m_host->resizeDocks({m_dock}, {m_host->width() / 2}, Qt::Horizontal);

        // Closing the dock with its own button unchecks View > 3D Workspace.
        connect(m_dock, &QDockWidget::visibilityChanged, this,
                [this](bool shown)
                {
                    if (!shown && !m_dock->isVisible())
                    {
                        const QSignalBlocker blocker(m_action);
                        m_action->setChecked(false);
                    }
                });
    }

    m_dock->setVisible(visible);
    if (visible)
    {
        m_dock->raise();
    }
}

} // namespace Seamly3D
