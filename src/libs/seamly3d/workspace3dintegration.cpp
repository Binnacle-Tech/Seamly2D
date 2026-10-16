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
#include <QCoreApplication>
#include <QMenuBar>
#include <QProcess>
#include <QOpenGLWidget>
#include <QSignalBlocker>

#include "dxfimport/dxfimportwizard.h"
#include "measurementguide.h"
#include "patternsource.h"
#include "workspace3dwindow.h"

namespace
{
// Object names of the host's View and Help menus (see mainwindow.ui). Looked up at runtime so the 2D UI file stays
// untouched.
const QString hostViewMenuName = QStringLiteral("view_Menu");
const QString hostHelpMenuName = QStringLiteral("help_Menu");
const QString hostFileMenuName = QStringLiteral("file_Menu");
const QString hostOpenActionName = QStringLiteral("actionOpen");
} // anonymous namespace

namespace Seamly3D
{

//---------------------------------------------------------------------------------------------------------------------
Workspace3DIntegration *Workspace3DIntegration::install(QMainWindow *host, const PatternOpener &openPattern)
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
    return new Workspace3DIntegration(host, openPattern);
}

//---------------------------------------------------------------------------------------------------------------------
Workspace3DIntegration::Workspace3DIntegration(QMainWindow *host, const PatternOpener &openPattern)
    : QObject(host)
    , m_host(host)
    , m_action(new QAction(tr("3D Workspace"), this))
    , m_openPattern(openPattern)
{
    // The 3D viewport is an OpenGL widget. Adding the first one to a window already on screen makes Qt destroy and
    // recreate that window (to draw it through OpenGL), so the whole main window would vanish and come back when the
    // workspace opens. A hidden OpenGL widget, there before the main window is first shown, makes it OpenGL capable
    // from the start. It is never shown, so it never creates an OpenGL context of its own.
    if (!host->isVisible())
    {
        auto *placeholder = new QOpenGLWidget(host);
        placeholder->setObjectName(QStringLiteral("seamly3dOpenGLPlaceholder"));
        placeholder->setFixedSize(0, 0);
        placeholder->hide();
    }

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

    // File > Import SVG or DXF Pattern..., right after Open.
    if (auto *fileMenu = host->findChild<QMenu *>(hostFileMenuName))
    {
        auto *importAction = new QAction(tr("Import SVG or DXF Pattern..."), this);
        importAction->setObjectName(QStringLiteral("seamly3dImportDxf_Action"));
        importAction->setToolTip(tr("Convert an SVG or DXF pattern (pattern sites, Inkscape, other pattern CAD) into a "
                                    "Seamly2D pattern"));
        connect(importAction, &QAction::triggered, this, &Workspace3DIntegration::importDxfPattern);
        QAction *before = nullptr;
        const QList<QAction *> actions = fileMenu->actions();
        for (int i = 0; i < actions.size(); ++i)
        {
            if (actions.at(i)->objectName() == hostOpenActionName)
            {
                before = actions.value(i + 1);
                break;
            }
        }
        fileMenu->insertAction(before, importAction);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void Workspace3DIntegration::importDxfPattern()
{
    DxfImportWizard wizard(PatternSource::defaultUnit(), m_host);
    connect(&wizard, &DxfImportWizard::patternConverted, this,
            [this](const QString &fileName, bool open)
            {
                if (!open)
                {
                    return;
                }
                // The host opens it (in a new window when it already has a pattern); without a host function, a new
                // Seamly2D does.
                if (m_openPattern)
                {
                    m_openPattern(fileName);
                }
                else
                {
                    QProcess::startDetached(QCoreApplication::applicationFilePath(), {fileName});
                }
            });
    wizard.exec();
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
