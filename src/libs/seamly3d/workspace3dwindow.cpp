/******************************************************************************
 *   @file   workspace3dwindow.cpp
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

#include "workspace3dwindow.h"

#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QToolBar>

#include "objmeshreader.h"
#include "viewport3d.h"

namespace Seamly3D
{

//---------------------------------------------------------------------------------------------------------------------
Workspace3DWindow::Workspace3DWindow(QWidget *parent)
    : QMainWindow(parent, Qt::Window)
    , m_viewport(new Viewport3D(this))
    , m_statusLabel(new QLabel(this))
{
    setObjectName(QStringLiteral("Seamly3DWorkspaceWindow"));
    setWindowTitle(tr("3D Workspace"));
    setCentralWidget(m_viewport);
    statusBar()->addWidget(m_statusLabel, 1);

    createActions();
    updateStatus();
    resize(1024, 768);
}

//---------------------------------------------------------------------------------------------------------------------
Viewport3D *Workspace3DWindow::viewport() const
{
    return m_viewport;
}

//---------------------------------------------------------------------------------------------------------------------
bool Workspace3DWindow::loadBodyModel(const QString &fileName)
{
    Mesh mesh;
    ObjMeshReader reader;
    if (!reader.readFile(fileName, mesh))
    {
        QMessageBox::warning(this, tr("Import body model"),
                             tr("Could not import body model %1.\n%2")
                                 .arg(QFileInfo(fileName).fileName(), reader.errorString()));
        return false;
    }

    m_bodyModelPath = fileName;
    m_viewport->setMesh(mesh);
    updateStatus();
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
void Workspace3DWindow::importBodyModel()
{
    const QString filter = tr("Wavefront OBJ (*.obj);;All files (*)");
    const QString fileName = QFileDialog::getOpenFileName(this, tr("Import body model"), m_lastDirectory, filter);
    if (fileName.isEmpty())
    {
        return;
    }

    m_lastDirectory = QFileInfo(fileName).absolutePath();
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    loadBodyModel(fileName);
    QGuiApplication::restoreOverrideCursor();
}

//---------------------------------------------------------------------------------------------------------------------
void Workspace3DWindow::clearBodyModel()
{
    m_bodyModelPath.clear();
    m_viewport->clearMesh();
    updateStatus();
}

//---------------------------------------------------------------------------------------------------------------------
void Workspace3DWindow::createActions()
{
    auto *importAction = new QAction(tr("&Import Body Model..."), this);
    importAction->setShortcut(QKeySequence::Open);
    importAction->setToolTip(tr("Import a 3D body model (avatar) from a Wavefront OBJ file"));
    connect(importAction, &QAction::triggered, this, &Workspace3DWindow::importBodyModel);

    m_clearAction = new QAction(tr("&Clear Body Model"), this);
    connect(m_clearAction, &QAction::triggered, this, &Workspace3DWindow::clearBodyModel);

    auto *closeAction = new QAction(tr("&Close"), this);
    closeAction->setShortcut(QKeySequence::Close);
    connect(closeAction, &QAction::triggered, this, &QWidget::close);

    auto *frameAction = new QAction(tr("&Frame All"), this);
    frameAction->setShortcut(QKeySequence(Qt::Key_F));
    connect(frameAction, &QAction::triggered, m_viewport, &Viewport3D::frameAll);

    auto *resetAction = new QAction(tr("&Reset View"), this);
    resetAction->setShortcut(QKeySequence(Qt::Key_Home));
    connect(resetAction, &QAction::triggered, m_viewport, &Viewport3D::resetView);

    auto *gridAction = new QAction(tr("Show &Grid"), this);
    gridAction->setCheckable(true);
    gridAction->setChecked(m_viewport->isGridVisible());
    connect(gridAction, &QAction::toggled, m_viewport, &Viewport3D::setGridVisible);

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(importAction);
    fileMenu->addAction(m_clearAction);
    fileMenu->addSeparator();
    fileMenu->addAction(closeAction);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(frameAction);
    viewMenu->addAction(resetAction);
    viewMenu->addSeparator();
    viewMenu->addAction(gridAction);

    QToolBar *toolBar = addToolBar(tr("3D Workspace"));
    toolBar->setObjectName(QStringLiteral("Seamly3DWorkspaceToolBar"));
    toolBar->addAction(importAction);
    toolBar->addSeparator();
    toolBar->addAction(frameAction);
    toolBar->addAction(resetAction);
    toolBar->addAction(gridAction);
}

//---------------------------------------------------------------------------------------------------------------------
void Workspace3DWindow::updateStatus()
{
    const Mesh &mesh = m_viewport->mesh();
    m_clearAction->setEnabled(!mesh.isEmpty());

    if (mesh.isEmpty())
    {
        m_statusLabel->setText(tr("No body model. Use File > Import Body Model to load an OBJ file. "
                                  "Drag to orbit, Shift/right drag to pan, wheel to zoom."));
        return;
    }

    const QVector3D size = mesh.bounds().size();
    m_statusLabel->setText(tr("%1 - %2 vertices, %3 triangles, size %4 x %5 x %6")
                               .arg(QFileInfo(m_bodyModelPath).fileName())
                               .arg(mesh.vertexCount())
                               .arg(mesh.triangleCount())
                               .arg(size.x(), 0, 'g', 4)
                               .arg(size.y(), 0, 'g', 4)
                               .arg(size.z(), 0, 'g', 4));
}

} // namespace Seamly3D
