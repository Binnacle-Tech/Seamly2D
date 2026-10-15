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
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>

#include "bodymeasurementspanel.h"
#include "meshslicer.h"
#include "objmeshreader.h"
#include "objmeshwriter.h"
#include "viewport3d.h"

namespace Seamly3D
{

//---------------------------------------------------------------------------------------------------------------------
Workspace3DWindow::Workspace3DWindow(QWidget *parent)
    : QMainWindow(parent, Qt::Window)
    , m_viewport(new Viewport3D(this))
    , m_measurementsPanel(new BodyMeasurementsPanel(this))
    , m_statusLabel(new QLabel(this))
    , m_regenerateTimer(new QTimer(this))
{
    setObjectName(QStringLiteral("Seamly3DWorkspaceWindow"));
    setWindowTitle(tr("3D Workspace"));
    setCentralWidget(m_viewport);
    statusBar()->addWidget(m_statusLabel, 1);

    auto *dock = new QDockWidget(tr("Body Measurements"), this);
    dock->setObjectName(QStringLiteral("Seamly3DBodyMeasurementsDock"));
    dock->setWidget(m_measurementsPanel);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    // Spin box edits arrive one at a time; regenerate once they settle.
    m_regenerateTimer->setSingleShot(true);
    m_regenerateTimer->setInterval(100);
    connect(m_regenerateTimer, &QTimer::timeout, this, &Workspace3DWindow::regenerateBody);
    connect(m_measurementsPanel, &BodyMeasurementsPanel::measurementsChanged, this,
            [this]()
            {
                m_source = BodySource::Parametric;
                m_regenerateTimer->start();
            });

    createActions();
    resize(1200, 800);

    m_body = ParametricBody::generate(m_measurementsPanel->measurements());
    m_viewport->setMesh(m_body.combined());
    updateStatus();
}

//---------------------------------------------------------------------------------------------------------------------
Viewport3D *Workspace3DWindow::viewport() const
{
    return m_viewport;
}

//---------------------------------------------------------------------------------------------------------------------
BodyMeasurementsPanel *Workspace3DWindow::measurementsPanel() const
{
    return m_measurementsPanel;
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

    m_regenerateTimer->stop();
    m_source = BodySource::Imported;
    m_bodyModelPath = fileName;
    m_viewport->setMesh(mesh);
    updateStatus();
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
void Workspace3DWindow::setBodyMeasurements(const BodyMeasurements &measurements)
{
    m_measurementsPanel->setMeasurements(measurements);
    m_regenerateTimer->stop();
    useParametricBody();
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
void Workspace3DWindow::exportBodyModel()
{
    const QString filter = tr("Wavefront OBJ (*.obj)");
    QString fileName = QFileDialog::getSaveFileName(this, tr("Export body model"), m_lastDirectory, filter);
    if (fileName.isEmpty())
    {
        return;
    }
    if (QFileInfo(fileName).suffix().isEmpty())
    {
        fileName += QLatin1String(".obj");
    }
    m_lastDirectory = QFileInfo(fileName).absolutePath();

    ObjMeshWriter writer;
    if (!writer.writeFile(fileName, m_viewport->mesh()))
    {
        QMessageBox::warning(this, tr("Export body model"), writer.errorString());
    }
}

//---------------------------------------------------------------------------------------------------------------------
void Workspace3DWindow::useParametricBody()
{
    const bool frame = m_source != BodySource::Parametric;
    m_source = BodySource::Parametric;
    m_bodyModelPath.clear();
    m_body = ParametricBody::generate(m_measurementsPanel->measurements());
    m_viewport->setMesh(m_body.combined(), frame);
    updateStatus();
}

//---------------------------------------------------------------------------------------------------------------------
void Workspace3DWindow::regenerateBody()
{
    // Keep the camera where the user put it while they tweak measurements.
    const bool frame = !m_bodyModelPath.isEmpty();
    m_bodyModelPath.clear();
    m_body = ParametricBody::generate(m_measurementsPanel->measurements());
    m_viewport->setMesh(m_body.combined(), frame);
    updateStatus();
}

//---------------------------------------------------------------------------------------------------------------------
void Workspace3DWindow::createActions()
{
    auto *loadMeasurementsAction = new QAction(tr("&Load Measurements..."), this);
    loadMeasurementsAction->setShortcut(QKeySequence::Open);
    loadMeasurementsAction->setToolTip(tr("Build the body from a SeamlyMe individual measurements (.vit) file"));
    connect(loadMeasurementsAction, &QAction::triggered, m_measurementsPanel, &BodyMeasurementsPanel::loadVitFile);

    auto *parametricAction = new QAction(tr("Body From &Measurements"), this);
    parametricAction->setToolTip(tr("Show the body built from the measurements"));
    connect(parametricAction, &QAction::triggered, this, &Workspace3DWindow::useParametricBody);

    auto *importAction = new QAction(tr("&Import Body Model..."), this);
    importAction->setToolTip(tr("Import a 3D body model (avatar) from a Wavefront OBJ file"));
    connect(importAction, &QAction::triggered, this, &Workspace3DWindow::importBodyModel);

    auto *exportAction = new QAction(tr("&Export Body Model..."), this);
    exportAction->setShortcut(QKeySequence::SaveAs);
    exportAction->setToolTip(tr("Save the body as a Wavefront OBJ file"));
    connect(exportAction, &QAction::triggered, this, &Workspace3DWindow::exportBodyModel);

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
    fileMenu->addAction(loadMeasurementsAction);
    fileMenu->addAction(parametricAction);
    fileMenu->addSeparator();
    fileMenu->addAction(importAction);
    fileMenu->addAction(exportAction);
    fileMenu->addSeparator();
    fileMenu->addAction(closeAction);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(frameAction);
    viewMenu->addAction(resetAction);
    viewMenu->addSeparator();
    viewMenu->addAction(gridAction);

    QToolBar *toolBar = addToolBar(tr("3D Workspace"));
    toolBar->setObjectName(QStringLiteral("Seamly3DWorkspaceToolBar"));
    toolBar->addAction(loadMeasurementsAction);
    toolBar->addAction(importAction);
    toolBar->addAction(exportAction);
    toolBar->addSeparator();
    toolBar->addAction(frameAction);
    toolBar->addAction(resetAction);
    toolBar->addAction(gridAction);
}

//---------------------------------------------------------------------------------------------------------------------
void Workspace3DWindow::updateStatus()
{
    const Mesh &mesh = m_viewport->mesh();

    if (m_source == BodySource::Imported)
    {
        const QVector3D size = mesh.bounds().size();
        m_statusLabel->setText(tr("%1 - %2 vertices, %3 triangles, size %4 x %5 x %6")
                                   .arg(QFileInfo(m_bodyModelPath).fileName())
                                   .arg(mesh.vertexCount())
                                   .arg(mesh.triangleCount())
                                   .arg(size.x(), 0, 'g', 4)
                                   .arg(size.y(), 0, 'g', 4)
                                   .arg(size.z(), 0, 'g', 4));
        return;
    }

    // Measure the model like a tape measure would, so the user can see it really fits.
    const BodyMeasurements &m = m_body.measurements;
    const auto girth = [this, &m](BodyMeasurement heightMeasurement)
    {
        return MeshSlicer::girthAtHeight(m_body.torso, static_cast<float>(m.value(heightMeasurement)));
    };
    m_statusLabel->setText(tr("Body from measurements - height %1 cm, measured on the model: bust %2 cm, "
                              "waist %3 cm, hip %4 cm. Drag to orbit, Shift/right drag to pan, wheel to zoom.")
                               .arg(m.value(BodyMeasurement::Height), 0, 'f', 1)
                               .arg(girth(BodyMeasurement::BustpointHeight), 0, 'f', 1)
                               .arg(girth(BodyMeasurement::WaistHeight), 0, 'f', 1)
                               .arg(girth(BodyMeasurement::HipHeight), 0, 'f', 1));
}

} // namespace Seamly3D
