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
#include <QComboBox>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QThread>
#include <QTimer>
#include <QToolBar>

#include <algorithm>
#include <memory>

#include "bodymeasurementspanel.h"
#include "meshslicer.h"
#include "objmeshreader.h"
#include "objmeshwriter.h"
#include "patternpiecespanel.h"
#include "patternprovider.h"
#include "piecearrangement.h"
#include "viewport3d.h"

namespace Seamly3D
{

//---------------------------------------------------------------------------------------------------------------------
Workspace3DWindow::Workspace3DWindow(QWidget *parent)
    : QMainWindow(parent, Qt::Window)
    , m_viewport(new Viewport3D(this))
    , m_measurementsPanel(new BodyMeasurementsPanel(this))
    , m_piecesPanel(new PatternPiecesPanel(this))
    , m_statusLabel(new QLabel(this))
    , m_regenerateTimer(new QTimer(this))
    , m_bodyKindComboBox(new QComboBox(this))
    , m_simulationTimer(new QTimer(this))
{
    setObjectName(QStringLiteral("Seamly3DWorkspaceWindow"));
    setWindowTitle(tr("3D Workspace"));
    setCentralWidget(m_viewport);
    statusBar()->addWidget(m_statusLabel, 1);

    auto *dock = new QDockWidget(tr("Body Measurements"), this);
    dock->setObjectName(QStringLiteral("Seamly3DBodyMeasurementsDock"));
    dock->setWidget(m_measurementsPanel);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    auto *piecesDock = new QDockWidget(tr("Pattern Pieces"), this);
    piecesDock->setObjectName(QStringLiteral("Seamly3DPatternPiecesDock"));
    piecesDock->setWidget(m_piecesPanel);
    addDockWidget(Qt::RightDockWidgetArea, piecesDock);
    tabifyDockWidget(dock, piecesDock);
    dock->raise();
    connect(m_piecesPanel, &PatternPiecesPanel::arrangementChanged, this, &Workspace3DWindow::updatePieces);
    connect(m_piecesPanel, &PatternPiecesPanel::useMeasurementsRequested, this,
            &Workspace3DWindow::usePatternMeasurements);
    connect(m_piecesPanel, &PatternPiecesPanel::simulationToggled, this, &Workspace3DWindow::setSimulationRunning);
    connect(m_piecesPanel, &PatternPiecesPanel::simulationResetRequested, this, &Workspace3DWindow::updatePieces);
    connect(m_piecesPanel, &PatternPiecesPanel::fabricChanged, this, &Workspace3DWindow::fabricChanged);
    connect(m_piecesPanel, &PatternPiecesPanel::seamsChanged, this, &Workspace3DWindow::seamsChanged);
    connect(m_piecesPanel, &PatternPiecesPanel::highlightChanged, this, &Workspace3DWindow::showSeamLines);
    connect(m_piecesPanel, &PatternPiecesPanel::fitMapChanged, this,
            [this]()
            {
                if (!m_simulator.isEmpty())
                {
                    showGarment();
                }
            });

    // The simulation advances a few frames at a time between repaints, so the window stays responsive.
    m_simulationTimer->setInterval(15);
    connect(m_simulationTimer, &QTimer::timeout, this, &Workspace3DWindow::stepSimulation);

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

    m_bodyKindComboBox->addItem(tr("Realistic (MakeHuman)"));
    m_bodyKindComboBox->addItem(tr("Simple mannequin"));
    m_bodyKindComboBox->setToolTip(tr("Body model fitted to the measurements"));
    if (MakeHumanData::instance(&m_realisticError).isNull())
    {
        m_bodyKindComboBox->setCurrentIndex(1);
        m_bodyKindComboBox->setEnabled(false);
        m_bodyKindComboBox->setToolTip(m_realisticError);
    }
    connect(m_bodyKindComboBox, &QComboBox::currentIndexChanged, this, [this]() { buildBody(false); });

    createActions();
    resize(1200, 800);

    buildBody(true);
}

//---------------------------------------------------------------------------------------------------------------------
Workspace3DWindow::~Workspace3DWindow()
{
    if (m_fitThread != nullptr)
    {
        m_fitThread->wait();
        delete m_fitThread;
    }
}

//---------------------------------------------------------------------------------------------------------------------
void Workspace3DWindow::setPatternProvider(const std::shared_ptr<PatternProvider> &provider)
{
    m_provider = provider;
    if (provider != nullptr)
    {
        m_measurementsPanel->setUnit(provider->defaultUnit()); // the unit the user works in, in 2D
    }
    m_piecesPanel->setProvider(provider);
    if (provider != nullptr && !m_piecesPanel->pieces().isEmpty())
    {
        m_viewport->frameAll();
    }
}

//---------------------------------------------------------------------------------------------------------------------
PatternPiecesPanel *Workspace3DWindow::piecesPanel() const
{
    return m_piecesPanel;
}

//---------------------------------------------------------------------------------------------------------------------
BodyFrame Workspace3DWindow::bodyFrame() const
{
    // Pieces wrap around the measured body; an imported model has no measurements, so they use the panel's.
    BodyFrame frame = BodyFrame::fromMeasurements(m_measurementsPanel->measurements());
    if (m_source == BodySource::Parametric)
    {
        // Sleeves follow the arm of the body shown: its pose depends on the body.
        frame = bodyKind() == BodyKind::Realistic && m_fit.valid ? frame.withArm(m_fit.leftShoulder, m_fit.leftWrist)
                                                                  : frame.withArm(m_body.leftShoulder, m_body.leftWrist);
    }
    return frame;
}

//---------------------------------------------------------------------------------------------------------------------
void Workspace3DWindow::updatePieces()
{
    // The pieces, their arrangement or the body changed: a garment sewn before no longer applies.
    resetSimulation();
    m_viewport->setOverlays(m_piecesPanel->overlays(bodyFrame()));
    m_piecesPanel->updateSeams(m_piecesPanel->panels(bodyFrame()));
    showSeamLines();
}

//---------------------------------------------------------------------------------------------------------------------
void Workspace3DWindow::seamsChanged()
{
    // Seams switched on, off, added or removed: sew again from the arranged pieces.
    const bool sewn = !m_simulator.isEmpty();
    resetSimulation();
    m_viewport->setOverlays(m_piecesPanel->overlays(bodyFrame()));
    showSeamLines();
    if (sewn)
    {
        m_piecesPanel->setSimulationRunning(true);
        setSimulationRunning(true);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void Workspace3DWindow::showSeamLines()
{
    // On the cloth while it drapes, otherwise on the pieces as arranged.
    const QVector<GarmentPanel> panels = m_simulator.isEmpty() ? m_piecesPanel->panels(bodyFrame())
                                                               : m_simulator.panels();
    const QVector<QPair<int, int>> highlighted = m_piecesPanel->highlightedEdges();
    QVector<Viewport3D::LineSet> lines;
    if (m_piecesPanel->showSeams())
    {
        QVector<QColor> colors;
        const QVector<Seam> seams = m_piecesPanel->seams(panels, &colors);
        if (!highlighted.isEmpty())
        {
            colors.fill(QColor(150, 150, 150)); // grayed, so the edges picked stand out
        }
        lines = PatternPiecesPanel::seamLines(panels, seams, colors);
    }
    for (const auto &edge : highlighted)
    {
        if (edge.first >= 0 && edge.first < panels.size() && edge.second < panels.at(edge.first).edgeCount())
        {
            Viewport3D::LineSet line = PatternPiecesPanel::edgeLine(panels.at(edge.first), edge.second,
                                                                    QColor(255, 235, 59), 5.0f);
            line.onTop = true;
            lines.append(line);
        }
    }
    m_viewport->setLines(lines);
}

//---------------------------------------------------------------------------------------------------------------------
void Workspace3DWindow::resetSimulation()
{
    m_simulationTimer->stop();
    m_simulator.clear();
    m_piecesPanel->setSimulationRunning(false);
    m_piecesPanel->setSimulationStatus(QString());
    m_piecesPanel->setSimulationDetails(QString());
}

//---------------------------------------------------------------------------------------------------------------------
void Workspace3DWindow::setSimulationRunning(bool run)
{
    if (!run)
    {
        m_simulationTimer->stop();
        if (!m_simulator.isEmpty())
        {
            m_piecesPanel->setSimulationStatus(tr("Paused."));
        }
        return;
    }

    if (m_simulator.isEmpty())
    {
        const QVector<GarmentPanel> panels = m_piecesPanel->panels(bodyFrame());
        const bool sewable = std::any_of(panels.cbegin(), panels.cend(),
                                         [](const GarmentPanel &panel) { return panel.sewable; });
        if (!sewable)
        {
            m_piecesPanel->setSimulationRunning(false);
            m_piecesPanel->setSimulationStatus(tr("Place some pieces on the body first (front, back, sides, sleeves)."));
            return;
        }

        const QVector<Seam> seams = m_piecesPanel->seams(panels);
        QStringList details;
        const auto edgeName = [&panels](int panel, int edge)
        {
            const GarmentPanel &p = panels.at(panel);
            return tr("%1%2, edge %3")
                .arg(p.name, p.copy > 0 ? tr(" (mirrored)") : QString())
                .arg(edge + 1);
        };
        for (const Seam &seam : seams)
        {
            details.append(edgeName(seam.panelA, seam.edgeA) + QStringLiteral(" \u2194 ")
                           + edgeName(seam.panelB, seam.edgeB));
        }

        m_simulator.setSettings(FabricPreset::preset(m_piecesPanel->fabric()).apply(ClothSettings()));
        m_simulator.setBody(m_viewport->mesh());
        m_simulator.setGarment(panels, seams);
        m_settledFrames = 0;
        m_piecesPanel->setSimulationStatus(tr("%n seam(s) to sew.", nullptr, static_cast<int>(seams.size())));
        m_piecesPanel->setSimulationDetails(details.join(QLatin1Char('\n')));
    }
    m_simulationTimer->start();
}

//---------------------------------------------------------------------------------------------------------------------
void Workspace3DWindow::showGarment()
{
    QVector<Viewport3D::Overlay> overlays = PatternPiecesPanel::overlays(m_simulator.panels());
    const FitMap::Kind kind = m_piecesPanel->fitMap();
    if (kind != FitMap::Kind::None)
    {
        const QVector<QVector<float>> values = kind == FitMap::Kind::Stretch ? m_simulator.strainMap()
                                                                             : m_simulator.easeMap();
        const QVector<GarmentPanel> panels = m_simulator.panels();
        for (int p = 0; p < overlays.size() && p < values.size(); ++p)
        {
            if (panels.at(p).sewable) // pieces hung beside the body are not worn
            {
                overlays[p].vertexColors = FitMap::colors(kind, values.at(p));
            }
        }
    }
    m_viewport->setOverlays(overlays);
    showSeamLines();
}

//---------------------------------------------------------------------------------------------------------------------
void Workspace3DWindow::fabricChanged()
{
    // Another fabric drapes differently: sew and drape again.
    if (m_simulator.isEmpty())
    {
        return;
    }
    updatePieces();
    m_piecesPanel->setSimulationRunning(true);
    setSimulationRunning(true);
}

//---------------------------------------------------------------------------------------------------------------------
void Workspace3DWindow::stepSimulation()
{
    if (m_simulator.isEmpty())
    {
        m_simulationTimer->stop();
        return;
    }

    // As many frames as fit in a short time slice, then show where the cloth is.
    QElapsedTimer slice;
    slice.start();
    do
    {
        m_simulator.step();
    } while (slice.elapsed() < 25);
    showGarment();

    const QString seams = tr("%n seam(s)", nullptr, m_simulator.seamCount());
    if (!m_simulator.isSewn())
    {
        m_piecesPanel->setSimulationStatus(
            tr("Sewing %1: widest gap %2 cm.").arg(seams).arg(m_simulator.maxSeamGap(), 0, 'f', 1));
        return;
    }

    // Draped once the cloth has stopped moving for a moment; then there is no need to keep computing.
    m_settledFrames = m_simulator.averageSpeed() < 0.5f ? m_settledFrames + 1 : 0;
    if (m_settledFrames >= 10)
    {
        m_simulationTimer->stop();
        m_piecesPanel->setSimulationRunning(false);
        m_piecesPanel->setSimulationStatus(
            tr("Draped (%1, %2 s of fabric time).").arg(seams).arg(m_simulator.time(), 0, 'f', 1));
        return;
    }
    m_piecesPanel->setSimulationStatus(
        tr("Draping (%1): %2 s.").arg(seams).arg(m_simulator.time(), 0, 'f', 1));
}

//---------------------------------------------------------------------------------------------------------------------
void Workspace3DWindow::usePatternMeasurements()
{
    if (m_provider == nullptr)
    {
        return;
    }

    BodyMeasurements measurements = m_measurementsPanel->measurements();
    const QStringList applied = m_provider->applyMeasurements(measurements);
    if (applied.isEmpty())
    {
        QMessageBox::information(this, tr("Use pattern's measurements"),
                                 tr("The pattern has none of the body measurements the 3D body uses. Load a "
                                    "measurement file for the pattern in Seamly2D first."));
        return;
    }

    m_measurementsPanel->setMeasurements(
        measurements,
        tr("The pattern's measurements: %n used, the rest estimated from the height.", "",
           static_cast<int>(applied.size())),
        applied.join(QLatin1String(", ")));
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
    buildBody(m_source != BodySource::Parametric);
}

//---------------------------------------------------------------------------------------------------------------------
void Workspace3DWindow::regenerateBody()
{
    // Keep the camera where the user put it while they tweak measurements.
    buildBody(!m_bodyModelPath.isEmpty());
}

//---------------------------------------------------------------------------------------------------------------------
void Workspace3DWindow::buildBody(bool frame)
{
    m_source = BodySource::Parametric;
    m_bodyModelPath.clear();

    if (bodyKind() == BodyKind::Realistic && !MakeHumanData::instance().isNull())
    {
        startFit(frame);
        return;
    }

    m_body = ParametricBody::generate(m_measurementsPanel->measurements());
    m_viewport->setMesh(m_body.combined(), frame);
    updatePieces();
    updateStatus();
}

//---------------------------------------------------------------------------------------------------------------------
void Workspace3DWindow::startFit(bool frame)
{
    if (m_fitThread != nullptr)
    {
        // One fit at a time; the latest measurements are fitted when this one is done.
        m_refitPending = true;
        m_framePending = m_framePending || frame;
        return;
    }

    m_statusLabel->setText(tr("Fitting the body to the measurements..."));

    const BodyMeasurements measurements = m_measurementsPanel->measurements();
    auto result = std::make_shared<MakeHumanFit>();
    m_fitThread = QThread::create([measurements, result]() { *result = MakeHumanFitter::fit(measurements); });
    connect(m_fitThread, &QThread::finished, this,
            [this, result, frame]()
            {
                m_fitThread->deleteLater();
                m_fitThread = nullptr;

                if (m_refitPending)
                {
                    m_refitPending = false;
                    const bool frameNext = frame || m_framePending;
                    m_framePending = false;
                    startFit(frameNext);
                    return;
                }

                m_fit = *result;
                if (!m_fit.valid)
                {
                    m_realisticError = m_fit.errorString;
                    m_body = ParametricBody::generate(m_measurementsPanel->measurements());
                    m_viewport->setMesh(m_body.combined(), frame);
                    updateStatus();
                    return;
                }
                showBody(frame);
            });
    m_fitThread->start();
}

//---------------------------------------------------------------------------------------------------------------------
void Workspace3DWindow::showBody(bool frame)
{
    if (m_source == BodySource::Imported || bodyKind() != BodyKind::Realistic)
    {
        return; // the user moved on while the fit was running
    }
    m_viewport->setMesh(m_fit.mesh, frame);
    updatePieces();
    updateStatus();
}

//---------------------------------------------------------------------------------------------------------------------
Workspace3DWindow::BodyKind Workspace3DWindow::bodyKind() const
{
    return m_bodyKindComboBox->currentIndex() == 0 ? BodyKind::Realistic : BodyKind::Mannequin;
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
    connect(closeAction, &QAction::triggered, this,
            [this]()
            {
                // Docked in Seamly2D's main window, close the dock, not just its contents.
                if (auto *dock = qobject_cast<QDockWidget *>(parentWidget()))
                {
                    dock->close();
                    return;
                }
                close();
            });

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

    auto *guideAction = new QAction(tr("&How to Measure"), this);
    guideAction->setShortcut(QKeySequence::HelpContents);
    guideAction->setToolTip(tr("Where on the body to take each measurement"));
    connect(guideAction, &QAction::triggered, m_measurementsPanel, &BodyMeasurementsPanel::showMeasuringGuide);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(guideAction);

    // Inside Seamly2D's main window (docked) these shortcuts only work while the 3D workspace has the focus, so they
    // never take the main window's own (Ctrl+O opens a pattern there).
    for (QAction *action : findChildren<QAction *>())
    {
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    }

    QToolBar *toolBar = addToolBar(tr("3D Workspace"));
    toolBar->setObjectName(QStringLiteral("Seamly3DWorkspaceToolBar"));
    toolBar->addWidget(new QLabel(tr("Body:"), toolBar));
    toolBar->addWidget(m_bodyKindComboBox);
    toolBar->addSeparator();
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
        m_statusLabel->setToolTip(QString());
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

    if (bodyKind() == BodyKind::Realistic && m_fit.valid)
    {
        // Report how well the body fits, with every measurement in the tooltip.
        QStringList lines;
        for (const MakeHumanFit::Entry &entry : m_fit.entries)
        {
            if (!entry.fitted)
            {
                lines.append(tr("%1: not used for this body").arg(BodyMeasurements::displayName(entry.measurement)));
                continue;
            }
            lines.append(tr("%1: %2 cm (wanted %3 cm)")
                             .arg(BodyMeasurements::displayName(entry.measurement))
                             .arg(entry.achieved, 0, 'f', 1)
                             .arg(entry.target, 0, 'f', 1));
        }
        m_statusLabel->setText(tr("Realistic body fitted to %1 measurements, largest difference %2 cm.")
                                   .arg(m_fit.entries.size() - m_fit.unfitted().size())
                                   .arg(m_fit.maxError(), 0, 'f', 2));
        m_statusLabel->setToolTip(lines.join(QLatin1Char('\n')));
        return;
    }
    m_statusLabel->setToolTip(QString());

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
