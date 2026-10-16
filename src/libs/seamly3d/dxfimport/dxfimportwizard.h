/******************************************************************************
 *   @file   dxfimportwizard.h
 **  @author Seamly2D project
 **  @date   16 10, 2026
 **
 **  @brief  Wizard that converts a DXF pattern into a parametric Seamly2D pattern.
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

#ifndef SEAMLY3D_DXFIMPORTWIZARD_H
#define SEAMLY3D_DXFIMPORTWIZARD_H

#include <QWidget>
#include <QWizard>

#include "dxfpattern.h"
#include "dxfpatternconverter.h"

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QTableWidget;
class QTimer;

namespace Seamly3D
{

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Draws one converted piece: the cut line, the seam line with its points (corners square, curve points round),
 * notches, internal lines, drill holes and the grain line.
 */
class DxfPiecePreview : public QWidget
{
    Q_OBJECT

public:
    explicit DxfPiecePreview(QWidget *parent = nullptr);

    void  setPiece(const PreparedPiece &piece);
    void  clear();
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    PreparedPiece m_piece{};
    bool          m_hasPiece{false};
};

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Converts a DXF pattern (AAMA / ASTM D6673 from another pattern CAD, or any DXF with closed outlines) into a
 * parametric Seamly2D pattern.
 *
 * Pages: the DXF file; its pieces (which to convert, their names, the file's units) with a preview; how to convert
 * them (units, how points are measured, seam allowance, curve accuracy); and where to save the pattern.
 */
class DxfImportWizard : public QWizard
{
    Q_OBJECT

public:
    explicit DxfImportWizard(const QString &patternUnit, QWidget *parent = nullptr);

    void    setFileName(const QString &fileName);
    QString outputFileName() const;
    bool    openWhenDone() const;

    /** @brief The options chosen so far (pieces, names, units...). */
    DxfImportOptions options() const;
    const DxfPattern &pattern() const;

    /** @brief readFile reads and analyses the DXF file; false with a message when it can not. */
    bool readFile(const QString &fileName, QString *error);

    /** @brief done converts the pattern when finishing (after the last page's checks), then reports it. */
    void done(int result) override;

signals:
    void patternConverted(const QString &fileName, bool open);

private:
    Q_DISABLE_COPY_MOVE(DxfImportWizard)

    friend class DxfFilePage;
    friend class DxfPiecesPage;
    friend class DxfOptionsPage;
    friend class DxfSavePage;

    bool convert(); ///< Writes the pattern; shows why when it can not
    void fillPieces();
    void updatePreview();
    void updateSummary();

    DxfPattern      m_pattern{};
    QString         m_dxfFileName{};
    QString         m_patternUnit;
    bool            m_converted{false};

    // File page
    QLineEdit      *m_fileEdit;
    QLabel         *m_fileStatus;
    // Pieces page
    QLabel         *m_patternSummary;
    QComboBox      *m_sourceUnitCombo;
    QTableWidget   *m_piecesTable;
    DxfPiecePreview *m_preview;
    // Options page
    QComboBox      *m_targetUnitCombo;
    QButtonGroup   *m_referenceGroup;
    QButtonGroup   *m_seamGroup;
    QDoubleSpinBox *m_allowanceSpin;
    QDoubleSpinBox *m_toleranceSpin;
    QDoubleSpinBox *m_cornerSpin;
    QLabel         *m_conversionSummary;
    DxfPiecePreview *m_optionsPreview;
    QTimer         *m_summaryTimer;
    // Save page
    QLineEdit      *m_outputEdit;
    QCheckBox      *m_openCheck;
};

} // namespace Seamly3D

#endif // SEAMLY3D_DXFIMPORTWIZARD_H
