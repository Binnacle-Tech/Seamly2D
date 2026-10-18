/******************************************************************************
 *   @file   dxfimportwizard.cpp
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

#include "dxfimportwizard.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWizardPage>

#include "dxfreader.h"
#include "svgreader.h"

namespace Seamly3D
{

namespace
{

enum PieceColumn
{
    IncludeColumn,
    NameColumn,
    QuantityColumn,
    ContentsColumn,
    PieceColumnCount
};

QString lastDirectory; // where the last DXF was opened, for this session

//---------------------------------------------------------------------------------------------------------------------
/** @brief Edits how many of a piece are cut: 1 to 99. */
class QuantityDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        auto *editor = new QSpinBox(parent);
        editor->setRange(1, 99);
        editor->setFrame(false);
        return editor;
    }
};

//---------------------------------------------------------------------------------------------------------------------
QString unitName(const QString &unit)
{
    if (unit == QLatin1String("mm"))
    {
        return DxfImportWizard::tr("Millimeters");
    }
    if (unit == QLatin1String("inch"))
    {
        return DxfImportWizard::tr("Inches");
    }
    if (unit == QLatin1String("m"))
    {
        return DxfImportWizard::tr("Meters");
    }
    if (unit == QLatin1String("ft"))
    {
        return DxfImportWizard::tr("Feet");
    }
    return DxfImportWizard::tr("Centimeters");
}

//---------------------------------------------------------------------------------------------------------------------
void fillUnits(QComboBox *combo, const QStringList &units)
{
    for (const QString &unit : units)
    {
        combo->addItem(unitName(unit), unit);
    }
}

} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
class DxfFilePage : public QWizardPage
{
public:
    explicit DxfFilePage(DxfImportWizard *wizard)
        : QWizardPage(wizard), m_wizard(wizard)
    {
        setTitle(DxfImportWizard::tr("Pattern file"));
        setSubTitle(DxfImportWizard::tr("Choose an SVG or DXF pattern: from a pattern site or Inkscape (SVG), from a "
                                        "pattern CAD system (AAMA or ASTM DXF), or any drawing of pattern pieces."));
        auto *layout = new QVBoxLayout(this);
        auto *row = new QHBoxLayout;
        row->addWidget(wizard->m_fileEdit, 1);
        auto *browse = new QPushButton(DxfImportWizard::tr("Browse..."), this);
        row->addWidget(browse);
        layout->addLayout(row);
        wizard->m_fileStatus->setWordWrap(true);
        layout->addWidget(wizard->m_fileStatus);
        layout->addStretch(1);

        connect(browse, &QPushButton::clicked, this,
                [this]()
                {
                    const QString file = QFileDialog::getOpenFileName(
                        this, DxfImportWizard::tr("Pattern file"),
                        lastDirectory.isEmpty() ? QDir::homePath() : lastDirectory,
                        DxfImportWizard::tr("Patterns (*.svg *.SVG *.dxf *.DXF);;SVG drawings (*.svg *.SVG);;"
                                            "DXF drawings (*.dxf *.DXF);;All files (*)"));
                    if (!file.isEmpty())
                    {
                        m_wizard->m_fileEdit->setText(QDir::toNativeSeparators(file));
                    }
                });
        connect(wizard->m_fileEdit, &QLineEdit::textChanged, this,
                [this]()
                {
                    m_wizard->m_fileStatus->clear();
                    emit completeChanged();
                });
    }

    bool isComplete() const override
    {
        return QFileInfo(m_wizard->m_fileEdit->text().trimmed()).isFile();
    }

    bool validatePage() override
    {
        QString error;
        if (!m_wizard->readFile(QDir::fromNativeSeparators(m_wizard->m_fileEdit->text().trimmed()), &error))
        {
            m_wizard->m_fileStatus->setText(QStringLiteral("<font color='#c62828'>%1</font>").arg(error.toHtmlEscaped()));
            return false;
        }
        return true;
    }

private:
    DxfImportWizard *m_wizard;
};

//---------------------------------------------------------------------------------------------------------------------
class DxfPiecesPage : public QWizardPage
{
public:
    explicit DxfPiecesPage(DxfImportWizard *wizard)
        : QWizardPage(wizard), m_wizard(wizard)
    {
        setTitle(DxfImportWizard::tr("Pieces"));
        setSubTitle(DxfImportWizard::tr("Choose the pieces to convert and check their names. Square points are "
                                        "corners, round ones are points the curves pass through."));
        auto *layout = new QVBoxLayout(this);
        wizard->m_patternSummary->setWordWrap(true);
        layout->addWidget(wizard->m_patternSummary);

        auto *units = new QFormLayout;
        units->addRow(DxfImportWizard::tr("Units in the file:"), wizard->m_sourceUnitCombo);
        layout->addLayout(units);

        auto *splitter = new QSplitter(this);
        splitter->addWidget(wizard->m_piecesTable);
        splitter->addWidget(wizard->m_preview);
        splitter->setStretchFactor(0, 3);
        splitter->setStretchFactor(1, 2);
        layout->addWidget(splitter, 1);

        connect(wizard->m_piecesTable, &QTableWidget::itemChanged, this, [this]() { emit completeChanged(); });
    }

    void initializePage() override { m_wizard->fillPieces(); }

    bool isComplete() const override
    {
        for (int row = 0; row < m_wizard->m_piecesTable->rowCount(); ++row)
        {
            const QTableWidgetItem *item = m_wizard->m_piecesTable->item(row, IncludeColumn);
            if (item != nullptr && item->checkState() == Qt::Checked)
            {
                return true;
            }
        }
        return false;
    }

private:
    DxfImportWizard *m_wizard;
};

//---------------------------------------------------------------------------------------------------------------------
class DxfOptionsPage : public QWizardPage
{
public:
    explicit DxfOptionsPage(DxfImportWizard *wizard)
        : QWizardPage(wizard), m_wizard(wizard)
    {
        setTitle(DxfImportWizard::tr("Conversion"));
        setSubTitle(DxfImportWizard::tr("Each piece becomes a draft block of points and curves you can edit like a "
                                        "pattern drafted in Seamly2D."));
        auto *layout = new QHBoxLayout(this);
        auto *form = new QFormLayout;
        form->addRow(DxfImportWizard::tr("Pattern units:"), wizard->m_targetUnitCombo);

        auto *fromPrevious = new QRadioButton(DxfImportWizard::tr("From the point before it"), this);
        fromPrevious->setToolTip(DxfImportWizard::tr("Change one length and the points after it follow"));
        auto *fromFirst = new QRadioButton(DxfImportWizard::tr("From the piece's first point"), this);
        fromFirst->setToolTip(DxfImportWizard::tr("Every point stays where it is when another one changes"));
        wizard->m_referenceGroup->addButton(fromPrevious, 0);
        wizard->m_referenceGroup->addButton(fromFirst, 1);
        fromPrevious->setChecked(true);
        auto *points = new QVBoxLayout;
        points->addWidget(fromPrevious);
        points->addWidget(fromFirst);
        form->addRow(DxfImportWizard::tr("Each point at a length and angle:"), points);

        auto *automatic = new QRadioButton(DxfImportWizard::tr("Sew line from the file, allowance up to the cut line"),
                                           this);
        automatic->setToolTip(DxfImportWizard::tr("Pieces without a sew line keep their cut line, allowance included"));
        auto *included = new QRadioButton(DxfImportWizard::tr("Cut line, allowance included"), this);
        auto *add = new QRadioButton(DxfImportWizard::tr("Cut line is the seam line, add:"), this);
        wizard->m_seamGroup->addButton(automatic, static_cast<int>(DxfImportOptions::SeamLine::Automatic));
        wizard->m_seamGroup->addButton(included,
                                       static_cast<int>(DxfImportOptions::SeamLine::CutLineIncludesAllowance));
        wizard->m_seamGroup->addButton(add, static_cast<int>(DxfImportOptions::SeamLine::CutLineAddAllowance));
        automatic->setChecked(true);
        auto *seam = new QVBoxLayout;
        seam->addWidget(automatic);
        seam->addWidget(included);
        auto *addRow = new QHBoxLayout;
        addRow->addWidget(add);
        addRow->addWidget(wizard->m_allowanceSpin);
        addRow->addStretch(1);
        seam->addLayout(addRow);
        form->addRow(DxfImportWizard::tr("Seam line:"), seam);

        form->addRow(DxfImportWizard::tr("Curves within:"), wizard->m_toleranceSpin);
        form->addRow(DxfImportWizard::tr("Corners sharper than:"), wizard->m_cornerSpin);
        wizard->m_conversionSummary->setWordWrap(true);
        wizard->m_conversionSummary->setTextFormat(Qt::RichText);
        form->addRow(wizard->m_conversionSummary);

        layout->addLayout(form, 3);
        layout->addWidget(wizard->m_optionsPreview, 2);
    }

    void initializePage() override
    {
        m_wizard->updatePreview();
        m_wizard->updateSummary();
    }

private:
    DxfImportWizard *m_wizard;
};

//---------------------------------------------------------------------------------------------------------------------
class DxfSavePage : public QWizardPage
{
public:
    explicit DxfSavePage(DxfImportWizard *wizard)
        : QWizardPage(wizard), m_wizard(wizard)
    {
        setTitle(DxfImportWizard::tr("Save"));
        setSubTitle(DxfImportWizard::tr("Where to save the Seamly2D pattern."));
        auto *layout = new QVBoxLayout(this);
        auto *row = new QHBoxLayout;
        row->addWidget(wizard->m_outputEdit, 1);
        auto *browse = new QPushButton(DxfImportWizard::tr("Browse..."), this);
        row->addWidget(browse);
        layout->addLayout(row);
        layout->addWidget(wizard->m_openCheck);
        layout->addStretch(1);

        connect(browse, &QPushButton::clicked, this,
                [this]()
                {
                    const QString file = QFileDialog::getSaveFileName(
                        this, DxfImportWizard::tr("Save pattern"), m_wizard->m_outputEdit->text(),
                        DxfImportWizard::tr("Seamly2D patterns (*.val)"));
                    if (!file.isEmpty())
                    {
                        m_wizard->m_outputEdit->setText(QDir::toNativeSeparators(file));
                        m_confirmed = QDir::fromNativeSeparators(file);
                    }
                });
        connect(wizard->m_outputEdit, &QLineEdit::textChanged, this, [this]() { emit completeChanged(); });
    }

    void initializePage() override
    {
        // Next to the DXF, with its name; kept when coming back, unless another DXF was chosen.
        if (m_wizard->m_outputEdit->text().isEmpty() || m_defaultFor != m_wizard->m_dxfFileName)
        {
            const QFileInfo dxf(m_wizard->m_dxfFileName);
            m_wizard->m_outputEdit->setText(QDir::toNativeSeparators(
                dxf.absolutePath() + QLatin1Char('/') + dxf.completeBaseName() + QStringLiteral(".val")));
            m_defaultFor = m_wizard->m_dxfFileName;
        }
    }

    bool isComplete() const override { return !m_wizard->m_outputEdit->text().trimmed().isEmpty(); }

    bool validatePage() override
    {
        QString file = QDir::fromNativeSeparators(m_wizard->m_outputEdit->text().trimmed());
        if (QFileInfo(file).suffix().isEmpty())
        {
            file += QStringLiteral(".val");
            m_wizard->m_outputEdit->setText(QDir::toNativeSeparators(file));
        }
        if (QFileInfo::exists(file) && file != m_confirmed
            && QMessageBox::question(this, DxfImportWizard::tr("Save pattern"),
                                     DxfImportWizard::tr("%1 already exists. Replace it?").arg(QFileInfo(file).fileName()))
                   != QMessageBox::Yes)
        {
            return false;
        }
        return m_wizard->convert();
    }

private:
    DxfImportWizard *m_wizard;
    QString          m_confirmed{};
    QString          m_defaultFor{};
};

//---------------------------------------------------------------------------------------------------------------------
DxfPiecePreview::DxfPiecePreview(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(220, 220);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

//---------------------------------------------------------------------------------------------------------------------
void DxfPiecePreview::setPiece(const PreparedPiece &piece)
{
    m_piece = piece;
    m_hasPiece = true;
    update();
}

//---------------------------------------------------------------------------------------------------------------------
void DxfPiecePreview::clear()
{
    m_hasPiece = false;
    update();
}

//---------------------------------------------------------------------------------------------------------------------
QSize DxfPiecePreview::sizeHint() const
{
    return QSize(320, 360);
}

//---------------------------------------------------------------------------------------------------------------------
void DxfPiecePreview::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), palette().base());
    if (!m_hasPiece || m_piece.outline.points.isEmpty())
    {
        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.drawText(rect(), Qt::AlignCenter, DxfImportWizard::tr("No piece selected"));
        return;
    }

    const QPainterPath seam = m_piece.outline.painterPath();
    QRectF bounds = seam.boundingRect().united(m_piece.cutLine.boundingRect());
    const double margin = 16;
    const double scale = qMin((width() - 2 * margin) / qMax(bounds.width(), 1e-9),
                              (height() - 2 * margin - 18) / qMax(bounds.height(), 1e-9));
    QTransform transform;
    transform.translate(width() / 2.0, (height() - 18) / 2.0);
    transform.scale(scale, scale);
    transform.translate(-bounds.center().x(), -bounds.center().y());

    const QColor text = palette().color(QPalette::Text);
    QColor cutFill = palette().color(QPalette::Highlight);
    cutFill.setAlpha(28);

    // Cut line, filled.
    if (!m_piece.cutLine.isEmpty())
    {
        QPainterPath cut;
        cut.addPolygon(transform.map(m_piece.cutLine));
        cut.closeSubpath();
        painter.fillPath(cut, cutFill);
        QPen pen(palette().color(QPalette::Mid), 1, Qt::DashLine);
        painter.strokePath(cut, pen);
    }

    // Seam line.
    painter.setPen(QPen(text, 1.6));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(transform.map(seam));

    // Internal lines and drill holes.
    painter.setPen(QPen(QColor(30, 110, 200), 1.2));
    for (const FittedPath &line : m_piece.internalLines)
    {
        painter.drawPath(transform.map(line.painterPath()));
    }
    for (const QPointF &hole : m_piece.drillHoles)
    {
        const QPointF p = transform.map(hole);
        painter.drawLine(p + QPointF(-4, -4), p + QPointF(4, 4));
        painter.drawLine(p + QPointF(-4, 4), p + QPointF(4, -4));
    }

    // Grain line with arrows.
    if (m_piece.hasGrainline)
    {
        const QLineF grain(transform.map(m_piece.grainline.p1()), transform.map(m_piece.grainline.p2()));
        painter.setPen(QPen(QColor(46, 125, 50), 1.4));
        painter.drawLine(grain);
        for (const QLineF &end : {grain, QLineF(grain.p2(), grain.p1())})
        {
            QLineF head(end.p2(), end.p1());
            head.setLength(8);
            QLineF left = head;
            left.setAngle(head.angle() + 25);
            QLineF right = head;
            right.setAngle(head.angle() - 25);
            painter.drawLine(left);
            painter.drawLine(right);
        }
    }

    // Points: corners square, curve points round, notches red.
    for (const FittedPoint &point : m_piece.outline.points)
    {
        const QPointF p = transform.map(point.position);
        if (point.notch)
        {
            painter.setPen(QPen(QColor(198, 40, 40), 1.4));
            painter.setBrush(QColor(198, 40, 40));
            painter.drawPolygon(QPolygonF({p + QPointF(0, 4), p + QPointF(-4, -4), p + QPointF(4, -4)}));
        }
        else if (point.corner)
        {
            painter.setPen(QPen(text, 1));
            painter.setBrush(palette().color(QPalette::Base));
            painter.drawRect(QRectF(p - QPointF(3, 3), QSizeF(6, 6)));
        }
        else
        {
            painter.setPen(QPen(text, 1));
            painter.setBrush(palette().color(QPalette::Highlight));
            painter.drawEllipse(p, 3, 3);
        }
    }

    painter.setPen(palette().color(QPalette::Text));
    painter.drawText(QRectF(4, height() - 20, width() - 8, 18), Qt::AlignLeft | Qt::AlignVCenter,
                     DxfImportWizard::tr("%1: %n point(s)", nullptr, static_cast<int>(m_piece.outline.points.size()))
                         .arg(m_piece.name));
}

//---------------------------------------------------------------------------------------------------------------------
DxfImportWizard::DxfImportWizard(const QString &patternUnit, QWidget *parent)
    : QWizard(parent)
    , m_patternUnit(patternUnit)
    , m_fileEdit(new QLineEdit(this))
    , m_fileStatus(new QLabel(this))
    , m_patternSummary(new QLabel(this))
    , m_sourceUnitCombo(new QComboBox(this))
    , m_piecesTable(new QTableWidget(this))
    , m_preview(new DxfPiecePreview(this))
    , m_targetUnitCombo(new QComboBox(this))
    , m_referenceGroup(new QButtonGroup(this))
    , m_seamGroup(new QButtonGroup(this))
    , m_allowanceSpin(new QDoubleSpinBox(this))
    , m_toleranceSpin(new QDoubleSpinBox(this))
    , m_cornerSpin(new QDoubleSpinBox(this))
    , m_conversionSummary(new QLabel(this))
    , m_optionsPreview(new DxfPiecePreview(this))
    , m_summaryTimer(new QTimer(this))
    , m_outputEdit(new QLineEdit(this))
    , m_openCheck(new QCheckBox(tr("Open the pattern when done"), this))
{
    setObjectName(QStringLiteral("seamly3dDxfImportWizard"));
    setWindowTitle(tr("Import SVG or DXF Pattern"));
    setWizardStyle(QWizard::ModernStyle);
    setOption(QWizard::NoBackButtonOnStartPage);
    resize(860, 600);

    fillUnits(m_sourceUnitCombo, {QStringLiteral("mm"), QStringLiteral("cm"), QStringLiteral("inch"),
                                  QStringLiteral("m"), QStringLiteral("ft")});
    fillUnits(m_targetUnitCombo, {QStringLiteral("cm"), QStringLiteral("mm"), QStringLiteral("inch")});
    const int unitIndex = m_targetUnitCombo->findData(patternUnit);
    m_targetUnitCombo->setCurrentIndex(unitIndex >= 0 ? unitIndex : 0);

    m_piecesTable->setColumnCount(PieceColumnCount);
    m_piecesTable->setHorizontalHeaderLabels({tr("Convert"), tr("Name"), tr("Quantity"), tr("Contents")});
    m_piecesTable->setItemDelegateForColumn(QuantityColumn, new QuantityDelegate(m_piecesTable));
    m_piecesTable->verticalHeader()->hide();
    m_piecesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_piecesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_piecesTable->horizontalHeader()->setSectionResizeMode(NameColumn, QHeaderView::Interactive);
    m_piecesTable->horizontalHeader()->setSectionResizeMode(ContentsColumn, QHeaderView::Stretch);
    m_piecesTable->setColumnWidth(NameColumn, 150);

    m_allowanceSpin->setRange(0, 100);
    m_allowanceSpin->setDecimals(2);
    m_allowanceSpin->setValue(patternUnit == QLatin1String("inch") ? 0.5 : patternUnit == QLatin1String("mm") ? 10 : 1);
    m_toleranceSpin->setRange(0.05, 10);
    m_toleranceSpin->setDecimals(2);
    m_toleranceSpin->setSingleStep(0.1);
    m_toleranceSpin->setValue(0.5);
    m_toleranceSpin->setSuffix(tr(" mm"));
    m_toleranceSpin->setToolTip(tr("How close the curves follow the file: closer needs more points"));
    m_cornerSpin->setRange(5, 120);
    m_cornerSpin->setDecimals(0);
    m_cornerSpin->setValue(30);
    m_cornerSpin->setSuffix(QStringLiteral("°"));
    m_cornerSpin->setToolTip(tr("Where the outline turns more than this, it is a corner"));
    m_openCheck->setChecked(true);

    m_summaryTimer->setSingleShot(true);
    m_summaryTimer->setInterval(250);
    connect(m_summaryTimer, &QTimer::timeout, this,
            [this]()
            {
                updatePreview();
                updateSummary();
            });
    const auto later = [this]() { m_summaryTimer->start(); };
    connect(m_sourceUnitCombo, &QComboBox::currentIndexChanged, this, later);
    connect(m_targetUnitCombo, &QComboBox::currentIndexChanged, this,
            [this]()
            {
                const QString unit = m_targetUnitCombo->currentData().toString();
                m_allowanceSpin->setSuffix(QLatin1Char(' ') + unit);
                m_summaryTimer->start();
            });
    m_allowanceSpin->setSuffix(QLatin1Char(' ') + m_targetUnitCombo->currentData().toString());
    connect(m_referenceGroup, &QButtonGroup::idClicked, this, later);
    connect(m_seamGroup, &QButtonGroup::idClicked, this, later);
    connect(m_allowanceSpin, &QDoubleSpinBox::valueChanged, this, later);
    connect(m_toleranceSpin, &QDoubleSpinBox::valueChanged, this, later);
    connect(m_cornerSpin, &QDoubleSpinBox::valueChanged, this, later);
    connect(m_piecesTable, &QTableWidget::currentCellChanged, this, [this]() { updatePreview(); });
    connect(m_piecesTable, &QTableWidget::itemChanged, this, later);

    addPage(new DxfFilePage(this));
    addPage(new DxfPiecesPage(this));
    addPage(new DxfOptionsPage(this));
    addPage(new DxfSavePage(this));
}

//---------------------------------------------------------------------------------------------------------------------
void DxfImportWizard::setFileName(const QString &fileName)
{
    m_fileEdit->setText(QDir::toNativeSeparators(fileName));
}

//---------------------------------------------------------------------------------------------------------------------
QString DxfImportWizard::outputFileName() const
{
    return QDir::fromNativeSeparators(m_outputEdit->text().trimmed());
}

//---------------------------------------------------------------------------------------------------------------------
bool DxfImportWizard::openWhenDone() const
{
    return m_openCheck->isChecked();
}

//---------------------------------------------------------------------------------------------------------------------
const DxfPattern &DxfImportWizard::pattern() const
{
    return m_pattern;
}

//---------------------------------------------------------------------------------------------------------------------
bool DxfImportWizard::readFile(const QString &fileName, QString *error)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (error != nullptr)
        {
            *error = tr("Could not open %1: %2").arg(fileName, file.errorString());
        }
        return false;
    }
    const QByteArray data = file.readAll();

    // SVG or DXF, by what the file holds rather than its name.
    DxfDocument document;
    bool read = false;
    QString message;
    if (SvgReader::isSvg(data))
    {
        SvgReader reader;
        read = reader.parse(data, document);
        message = reader.errorString();
    }
    else
    {
        DxfReader reader;
        read = reader.parse(data, document);
        message = reader.errorString();
    }
    if (!read)
    {
        if (error != nullptr)
        {
            *error = message;
        }
        return false;
    }
    DxfPattern pattern = DxfPatternReader::extract(document);
    if (pattern.pieces.isEmpty())
    {
        if (error != nullptr)
        {
            *error = pattern.warnings.join(QLatin1Char('\n'));
        }
        return false;
    }
    m_pattern = pattern;
    m_dxfFileName = fileName;
    lastDirectory = QFileInfo(fileName).absolutePath();
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
void DxfImportWizard::fillPieces()
{
    const QSignalBlocker blocker(m_piecesTable);
    m_piecesTable->setRowCount(static_cast<int>(m_pattern.pieces.size()));
    for (int row = 0; row < m_pattern.pieces.size(); ++row)
    {
        const DxfPiece &piece = m_pattern.pieces.at(row);
        auto *include = new QTableWidgetItem;
        include->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        include->setCheckState(Qt::Checked);
        m_piecesTable->setItem(row, IncludeColumn, include);
        m_piecesTable->setItem(row, NameColumn, new QTableWidgetItem(piece.name));
        auto *quantity = new QTableWidgetItem;
        quantity->setData(Qt::EditRole, qMax(1, piece.quantity));
        quantity->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
        quantity->setToolTip(tr("How many are cut (double-click to change). Two or more: a pair, mirrored."));
        m_piecesTable->setItem(row, QuantityColumn, quantity);

        QStringList contents;
        contents << (piece.sewLine.isEmpty() ? tr("cut line") : tr("cut and sew lines"));
        if (!piece.notches.isEmpty())
        {
            contents << tr("%n notch(es)", nullptr, static_cast<int>(piece.notches.size()));
        }
        if (piece.hasGrainline)
        {
            contents << tr("grain line");
        }
        if (piece.hasMirrorLine)
        {
            contents << tr("on fold");
        }
        if (!piece.internalLines.isEmpty())
        {
            contents << tr("%n internal line(s)", nullptr, static_cast<int>(piece.internalLines.size()));
        }
        if (!piece.drillHoles.isEmpty())
        {
            contents << tr("%n drill hole(s)", nullptr, static_cast<int>(piece.drillHoles.size()));
        }
        auto *details = new QTableWidgetItem(contents.join(QStringLiteral(", ")));
        details->setToolTip(details->text());
        details->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_piecesTable->setItem(row, ContentsColumn, details);
    }
    m_piecesTable->resizeColumnToContents(IncludeColumn);
    m_piecesTable->resizeColumnToContents(QuantityColumn);

    {
        const QSignalBlocker unitBlocker(m_sourceUnitCombo);
        const int index = m_sourceUnitCombo->findData(m_pattern.unit);
        m_sourceUnitCombo->setCurrentIndex(index >= 0 ? index : 0);
    }
    QStringList summary;
    summary << tr("%1: %n piece(s).", nullptr, static_cast<int>(m_pattern.pieces.size()))
                   .arg(m_pattern.astm ? tr("AAMA/ASTM pattern") : tr("Drawing"));
    summary << tr("Units: %1, %2.").arg(unitName(m_pattern.unit).toLower(), m_pattern.unitSource);
    for (const QString &warning : qAsConst(m_pattern.warnings))
    {
        summary << warning;
    }
    m_patternSummary->setText(summary.join(QLatin1Char(' ')));

    m_piecesTable->setCurrentCell(0, NameColumn);
    updatePreview();
}

//---------------------------------------------------------------------------------------------------------------------
DxfImportOptions DxfImportWizard::options() const
{
    DxfImportOptions options;
    options.sourceUnit = m_sourceUnitCombo->currentData().toString();
    options.targetUnit = m_targetUnitCombo->currentData().toString();
    options.fromPreviousPoint = m_referenceGroup->checkedId() != 1;
    options.seamLine = static_cast<DxfImportOptions::SeamLine>(qMax(0, m_seamGroup->checkedId()));
    options.seamAllowance = m_allowanceSpin->value();
    options.curveTolerance = m_toleranceSpin->value() / 10.0; // mm to cm
    options.cornerAngle = m_cornerSpin->value();
    for (int row = 0; row < m_piecesTable->rowCount(); ++row)
    {
        const QTableWidgetItem *include = m_piecesTable->item(row, IncludeColumn);
        const QTableWidgetItem *name = m_piecesTable->item(row, NameColumn);
        const QTableWidgetItem *quantity = m_piecesTable->item(row, QuantityColumn);
        options.include.append(include != nullptr && include->checkState() == Qt::Checked);
        options.names.append(name != nullptr ? name->text() : QString());
        options.quantities.append(quantity != nullptr ? qMax(1, quantity->data(Qt::EditRole).toInt()) : 0);
    }
    const QFileInfo dxf(m_dxfFileName);
    options.patternName = m_pattern.styleName.isEmpty() ? dxf.completeBaseName() : m_pattern.styleName;
    options.description = tr("Converted from %1 (%2, %3).")
                              .arg(dxf.fileName(), m_pattern.astm ? tr("AAMA/ASTM pattern") : tr("drawing"),
                                   unitName(options.sourceUnit).toLower());
    return options;
}

//---------------------------------------------------------------------------------------------------------------------
void DxfImportWizard::updatePreview()
{
    const int row = m_piecesTable->currentRow();
    if (row < 0 || row >= m_pattern.pieces.size())
    {
        m_preview->clear();
        m_optionsPreview->clear();
        return;
    }
    DxfImportOptions only = options();
    only.include = QVector<bool>(static_cast<int>(m_pattern.pieces.size()), false);
    only.include[row] = true;
    const QVector<PreparedPiece> pieces = DxfPatternConverter::prepare(m_pattern, only);
    if (pieces.isEmpty())
    {
        m_preview->clear();
        m_optionsPreview->clear();
        return;
    }
    m_preview->setPiece(pieces.constFirst());
    m_optionsPreview->setPiece(pieces.constFirst());
}

//---------------------------------------------------------------------------------------------------------------------
void DxfImportWizard::updateSummary()
{
    const DxfImportOptions current = options();
    const QVector<PreparedPiece> pieces = DxfPatternConverter::prepare(m_pattern, current);
    int points = 0;
    int curves = 0;
    double worst = 0;
    QStringList lines;
    for (const PreparedPiece &piece : pieces)
    {
        points += static_cast<int>(piece.outline.points.size());
        curves += piece.outline.curveCount();
        worst = qMax(worst, piece.outline.maxDeviation);
    }
    const double worstMm = worst * DxfPattern::toCentimeters(current.targetUnit) * 10;
    lines << tr("%n piece(s)", nullptr, static_cast<int>(pieces.size()))
          << tr("%n point(s)", nullptr, points)
          << tr("%n curve segment(s)", nullptr, curves)
          << tr("curves within %1 mm of the file").arg(worstMm, 0, 'f', 2);
    m_conversionSummary->setText(QStringLiteral("<b>%1</b>").arg(lines.join(QStringLiteral(", ")).toHtmlEscaped()));
}

//---------------------------------------------------------------------------------------------------------------------
bool DxfImportWizard::convert()
{
    QString error;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_converted = DxfPatternConverter::convert(m_pattern, options(), outputFileName(), &error);
    QApplication::restoreOverrideCursor();
    if (!m_converted)
    {
        QMessageBox::warning(this, windowTitle(), error);
    }
    return m_converted;
}

//---------------------------------------------------------------------------------------------------------------------
void DxfImportWizard::done(int result)
{
    m_converted = false;
    QWizard::done(result); // checks the last page, which converts
    if (result == QDialog::Accepted && m_converted)
    {
        emit patternConverted(outputFileName(), openWhenDone());
    }
}

} // namespace Seamly3D
