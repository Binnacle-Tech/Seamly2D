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

#include <memory>

#include "clothsimulator.h"
#include "garment.h"
#include "makehuman/makehumanfitter.h"
#include "parametricbody.h"
#include "piecearrangement.h"

class QAction;
class QComboBox;
class QLabel;
class QThread;
class QTimer;

namespace Seamly3D
{

class BodyMeasurementsPanel;
class PatternPiecesPanel;
class PatternProvider;
class Viewport3D;

/**
 * @brief Top level window of the 3D workspace.
 *
 * Shows a body (avatar) fitted to the user's measurements, or an imported body model, in a 3D viewport. The fitted
 * body is the realistic MakeHuman body when its data is available, or the simple mannequin. It is
 * independent of the 2D main window so the 3D workspace can grow (garments, sewing, simulation) without touching the
 * 2D code.
 */
class Workspace3DWindow : public QMainWindow
{
    Q_OBJECT

public:
    /** @brief nearestEdge returns the edge of @p panel nearest to @p point (-1 if it has none). */
    static int  nearestEdge(const GarmentPanel &panel, const QVector3D &point);

    explicit Workspace3DWindow(QWidget *parent = nullptr);
    ~Workspace3DWindow() override;

    Viewport3D            *viewport() const;
    BodyMeasurementsPanel *measurementsPanel() const;

    bool        loadBodyModel(const QString &fileName);
    void        setBodyMeasurements(const BodyMeasurements &measurements);

    /** @brief setPatternProvider shows that pattern's pieces around the body (the application gives the open one). */
    void        setPatternProvider(const std::shared_ptr<PatternProvider> &provider);
    PatternPiecesPanel *piecesPanel() const;

public slots:
    void        importBodyModel();
    void        exportBodyModel();
    void        useParametricBody();

private slots:
    void        regenerateBody();
    void        updatePieces();
    void        usePatternMeasurements();
    void        setSimulationRunning(bool run);
    void        resetSimulation();
    void        stepSimulation();
    void        fabricChanged();
    void        showGarment();
    void        seamsChanged();
    void        pieceDragged(int index, const QVector3D &point, const QVector3D &delta);
    void        edgeClicked(int index, const QVector3D &point);
    void        showSeamLines();

private:
    Q_DISABLE_COPY_MOVE(Workspace3DWindow)

    enum class BodySource : char { Parametric, Imported };
    enum class BodyKind : char { Realistic, Mannequin };

    void        createActions();
    void        updateStatus();
    void        buildBody(bool frame);
    void        startFit(bool frame);
    void        showBody(bool frame);
    BodyKind    bodyKind() const;
    BodyFrame   bodyFrame() const;

    Viewport3D            *m_viewport;
    BodyMeasurementsPanel *m_measurementsPanel;
    PatternPiecesPanel    *m_piecesPanel;
    std::shared_ptr<PatternProvider> m_provider{};
    QLabel                *m_statusLabel;
    QTimer                *m_regenerateTimer;
    QComboBox             *m_bodyKindComboBox;
    QTimer                *m_simulationTimer;
    ClothSimulator         m_simulator{};
    QVector<GarmentPanel>  m_arranged{};   ///< the pieces as arranged, as shown before sewing
    QPair<int, int>        m_firstEdge{-1, -1}; ///< sewing by clicking: the edge clicked first (panel, edge)
    int                    m_settledFrames{0};

    BodySource  m_source{BodySource::Parametric};
    BodyModel   m_body{};          ///< Simple mannequin
    MakeHumanFit m_fit{};          ///< Realistic body
    QString     m_realisticError{};
    QThread    *m_fitThread{nullptr};   ///< Fits run in the background so typing stays responsive
    bool        m_refitPending{false};  ///< Measurements changed while a fit was running
    bool        m_framePending{false};
    QString     m_bodyModelPath{};
    QString     m_lastDirectory{};
};

} // namespace Seamly3D

#endif // SEAMLY3D_WORKSPACE3DWINDOW_H
