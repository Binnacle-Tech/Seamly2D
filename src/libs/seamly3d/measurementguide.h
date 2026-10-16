/******************************************************************************
 *   @file   measurementguide.h
 **  @author Seamly2D project
 **  @date   15 10, 2026
 **
 **  @brief  How to take each body measurement: diagram and description.
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

#ifndef SEAMLY3D_MEASUREMENTGUIDE_H
#define SEAMLY3D_MEASUREMENTGUIDE_H

#include <QDialog>
#include <QWidget>

#include "bodymeasurements.h"

class QLabel;
class QListWidget;

namespace Seamly3D
{

/**
 * @brief Shows how to take one body measurement: the Seamly2D measurement diagram, its name, number and description.
 */
class MeasurementGuideWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MeasurementGuideWidget(int pictureHeight, QWidget *parent = nullptr);
    ~MeasurementGuideWidget() override = default;

    BodyMeasurement measurement() const;

public slots:
    void showMeasurement(Seamly3D::BodyMeasurement measurement);

private:
    Q_DISABLE_COPY_MOVE(MeasurementGuideWidget)

    BodyMeasurement m_measurement{BodyMeasurement::Height};
    int              m_pictureHeight;
    QLabel          *m_picture;
    QLabel          *m_title;
    QLabel          *m_description;
};

/**
 * @brief "How to Measure": every body measurement of the 3D workspace with its picture and description.
 */
class MeasurementGuideDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MeasurementGuideDialog(QWidget *parent = nullptr);
    ~MeasurementGuideDialog() override = default;

public slots:
    void showMeasurement(Seamly3D::BodyMeasurement measurement);

private:
    Q_DISABLE_COPY_MOVE(MeasurementGuideDialog)

    QListWidget            *m_list;
    MeasurementGuideWidget *m_guide;
};

} // namespace Seamly3D

#endif // SEAMLY3D_MEASUREMENTGUIDE_H
