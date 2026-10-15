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

#include "parametricbody.h"

class QAction;
class QLabel;
class QTimer;

namespace Seamly3D
{

class BodyMeasurementsPanel;
class Viewport3D;

/**
 * @brief Top level window of the 3D workspace.
 *
 * Shows a body (avatar) built from the user's measurements, or an imported body model, in a 3D viewport. It is
 * independent of the 2D main window so the 3D workspace can grow (garments, sewing, simulation) without touching the
 * 2D code.
 */
class Workspace3DWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit Workspace3DWindow(QWidget *parent = nullptr);
    ~Workspace3DWindow() override = default;

    Viewport3D            *viewport() const;
    BodyMeasurementsPanel *measurementsPanel() const;

    bool        loadBodyModel(const QString &fileName);
    void        setBodyMeasurements(const BodyMeasurements &measurements);

public slots:
    void        importBodyModel();
    void        exportBodyModel();
    void        useParametricBody();

private slots:
    void        regenerateBody();

private:
    Q_DISABLE_COPY_MOVE(Workspace3DWindow)

    enum class BodySource : char { Parametric, Imported };

    void        createActions();
    void        updateStatus();

    Viewport3D            *m_viewport;
    BodyMeasurementsPanel *m_measurementsPanel;
    QLabel                *m_statusLabel;
    QTimer                *m_regenerateTimer;

    BodySource  m_source{BodySource::Parametric};
    BodyModel   m_body{};
    QString     m_bodyModelPath{};
    QString     m_lastDirectory{};
};

} // namespace Seamly3D

#endif // SEAMLY3D_WORKSPACE3DWINDOW_H
