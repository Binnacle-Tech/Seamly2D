/******************************************************************************
 *   @file   bodymeasurementspanel.h
 **  @author Seamly2D project
 **  @date   15 10, 2026
 **
 **  @brief  Form for entering body measurements in the 3D workspace.
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

#ifndef SEAMLY3D_BODYMEASUREMENTSPANEL_H
#define SEAMLY3D_BODYMEASUREMENTSPANEL_H

#include <QVector>
#include <QWidget>

#include "bodymeasurements.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSlider;
class QSpinBox;

class QEvent;

namespace Seamly3D
{

class MeasurementGuideWidget;

/**
 * @brief Form where the user enters their body measurements (or loads them from a SeamlyMe .vit file).
 *
 * Values are edited in centimeters or inches; measurements() always returns centimeters.
 */
class BodyMeasurementsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit BodyMeasurementsPanel(QWidget *parent = nullptr);
    ~BodyMeasurementsPanel() override = default;

    BodyMeasurements measurements() const;
    void             setMeasurements(const BodyMeasurements &measurements);
    /** @brief setMeasurements also says where they come from, e.g. "The pattern's measurements". */
    void             setMeasurements(const BodyMeasurements &measurements, const QString &source,
                                     const QString &details = QString());

signals:
    void measurementsChanged(const Seamly3D::BodyMeasurements &measurements);

public slots:
    void loadVitFile();
    void resetToDefaults();
    void showMeasuringGuide(); ///< Opens "How to Measure" at the measurement being edited.

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void valueEdited(int index);
    void buildEdited();
    void unitChanged();

private:
    Q_DISABLE_COPY_MOVE(BodyMeasurementsPanel)

    double unitFactor() const; ///< Centimeters per displayed unit.
    void   updateSpinBoxes();

    BodyMeasurements         m_measurements{};
    QVector<QDoubleSpinBox *> m_spinBoxes{};
    QComboBox               *m_unitComboBox;
    QComboBox               *m_genderComboBox;
    QSlider                 *m_muscleSlider;
    QSpinBox                *m_ageSpinBox;
    QCheckBox               *m_proportionalCheckBox;
    QLabel                  *m_sourceLabel;
    MeasurementGuideWidget  *m_guide;
    QString                  m_lastDirectory{};
    bool                     m_updating{false};
};

} // namespace Seamly3D

#endif // SEAMLY3D_BODYMEASUREMENTSPANEL_H
