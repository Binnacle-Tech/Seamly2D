/******************************************************************************
 *   @file   patternpiecespanel.h
 **  @author Seamly2D project
 **  @date   15 10, 2026
 **
 **  @brief  The pattern's pieces in the 3D workspace.
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

#ifndef SEAMLY3D_PATTERNPIECESPANEL_H
#define SEAMLY3D_PATTERNPIECESPANEL_H

#include <QHash>
#include <QSet>
#include <QVector>
#include <QWidget>

#include <memory>

#include "fitmap.h"
#include "garment.h"
#include "piecearrangement.h"
#include "piecemesher.h"
#include "viewport3d.h"
#include "workspacesetup.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QTimer;
class QTreeWidget;

namespace Seamly3D
{

class PatternProvider;

/**
 * @brief The pattern's pieces in the 3D workspace: which ones show, and where each starts around the body.
 */
class PatternPiecesPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PatternPiecesPanel(QWidget *parent = nullptr);
    ~PatternPiecesPanel() override;

    void setProvider(const std::shared_ptr<PatternProvider> &provider);
    bool hasProvider() const;

    /** @brief Per piece arrangement, kept by piece id across reloads. */
    struct Settings
    {
        PiecePlacement  placement{PiecePlacement::Aside};
        bool            pair{false};
        bool            flip{false};
        PieceAdjustment adjustment{}; ///< moved by hand from where the placement puts it
    };

    void     setPieces(const QVector<PatternPiece> &pieces);
    QVector<PatternPiece> pieces() const;
    Settings settings(quint32 id) const;
    void     setSettings(quint32 id, const Settings &settings);
    /**
     * @brief nudge moves a piece by hand: around its place by @p around degrees and up by @p lift cm, as seen on its
     * copy @p copy (the mirrored copy moves the mirrored way, so a drag on it moves it where the mouse goes).
     */
    void     nudge(quint32 id, int copy, float around, float lift);
    /** @brief selectPiece shows the piece in the list, to adjust it there. */
    void     selectPiece(quint32 id);
    quint32  selectedPiece() const; ///< 0 when none

    /** @brief panels returns the pieces (and their mirrored copies) placed around a body of these sizes. */
    QVector<GarmentPanel> panels(const BodyFrame &frame) const;

    /** @brief overlays returns the piece meshes placed around a body of these sizes, one color per piece. */
    QVector<Viewport3D::Overlay> overlays(const BodyFrame &frame) const;
    static QVector<Viewport3D::Overlay> overlays(const QVector<GarmentPanel> &panels);

    static constexpr double meshEdge = 2.5; ///< centimeters between mesh points of the pieces

    /** @brief setSimulationRunning shows whether the garment is being sewn and draped (the button's state). */
    void     setSimulationRunning(bool running);
    void     setSimulationStatus(const QString &text);
    void     setSimulationDetails(const QString &details); ///< shown as the status' tooltip (the seams found)

    /** @brief The fabric the garment is made of (FabricPreset id). */
    QString      fabric() const;
    void         setFabric(const QString &id);
    /** @brief How the draped garment is colored. */
    FitMap::Kind fitMap() const;
    void         setFitMap(FitMap::Kind kind);

    using SeamSpec = Seamly3D::SeamSpec;

    /** @brief The setup to keep (pieces, seams, fabric, fit map), and putting a kept one back. */
    WorkspaceSetup setup() const;
    void           applySetup(const WorkspaceSetup &setup);
    /** @brief setupFile is where the setup is kept: next to the pattern; empty when the pattern is not saved yet. */
    QString        setupFile() const;
    /** @brief saveSetup writes the setup now (it is written by itself shortly after every change). */
    bool           saveSetup(QString *error = nullptr);

    /**
     * @brief updateSeams finds the seams of the garment as placed in @p panels (from panels()). Seams switched off
     * stay off, seams added by hand stay.
     */
    void         updateSeams(const QVector<GarmentPanel> &panels);
    /** @brief The seams to sew between @p panels, and the color each is drawn in. */
    QVector<Seam> seams(const QVector<GarmentPanel> &panels, QVector<QColor> *colors = nullptr) const;
    void         addSeam(const SeamSpec &spec);
    /**
     * @brief sewEdges adds a seam between two whole edges, by panel and edge number in the panels of the last
     * updateSeams(), their ends joined the way they lie closest. Does nothing for an edge with itself.
     */
    void         sewEdges(int panelA, int edgeA, int panelB, int edgeB);
    /** @brief Edges shown highlighted, as picked by clicking: panel and edge numbers. */
    void         setHighlightedEdges(const QVector<QPair<int, int>> &edges);
    bool         isSewingByClicking() const;
    void         setSewingByClicking(bool on);
    QVector<SeamSpec> foundSeams() const;
    bool         showSeams() const;
    /** @brief Edges to show highlighted (panel and edge numbers in the panels of the last updateSeams()). */
    QVector<QPair<int, int>> highlightedEdges() const;

    /** @brief The lines showing @p seams on @p panels: both edges, and ties between the points that meet. */
    static QVector<Viewport3D::LineSet> seamLines(const QVector<GarmentPanel> &panels, const QVector<Seam> &seams,
                                                  const QVector<QColor> &colors);
    static Viewport3D::LineSet edgeLine(const GarmentPanel &panel, int edge, const QColor &color, float width,
                                        double from = 0.0, double to = 1.0);

signals:
    void arrangementChanged();
    void useMeasurementsRequested();
    void simulationToggled(bool run);
    void simulationResetRequested();
    void fabricChanged();
    void seamsChanged();
    void highlightChanged();
    void fitMapChanged();
    void setupChanged(); ///< the user changed something kept in the setup file
    void sewingByClickingToggled(bool on);

public slots:
    void reload();

private:
    Q_DISABLE_COPY_MOVE(PatternPiecesPanel)

    void rebuildList();
    void showAdjustment();
    void changed(); ///< something kept in the setup changed: save it soon
    void rebuildSeamList();
    void addSeamDialog();
    int  panelIndex(quint32 piece, int copy) const;
    QString edgeName(int panel, int edge) const;
    /** @brief A piece's flat mesh: its first outlineCount vertices are its outline, corners among them. */
    struct FlatPiece
    {
        Mesh         mesh{};
        int          outlineCount{0};
        QVector<int> corners{};
    };
    const FlatPiece &flatPiece(const PatternPiece &piece) const;

    std::shared_ptr<PatternProvider> m_provider{};
    QVector<PatternPiece>            m_pieces{};
    QHash<quint32, Settings>         m_settings{};
    mutable QHash<quint32, FlatPiece> m_flatPieces{};

    QTreeWidget *m_list;
    QWidget        *m_adjustBox;
    QDoubleSpinBox *m_liftSpin;
    QDoubleSpinBox *m_aroundSpin;
    QDoubleSpinBox *m_turnSpin;
    QPushButton    *m_resetAdjustButton;
    QLabel      *m_status;
    QPushButton *m_reloadButton;
    QPushButton *m_measurementsButton;
    QCheckBox   *m_followCheckBox;
    QPushButton *m_simulateButton;
    QPushButton *m_resetButton;
    QLabel      *m_simulationStatus;
    QComboBox   *m_fabricCombo;
    QComboBox   *m_fitMapCombo;
    QWidget     *m_legend;
    QLabel      *m_legendBar;
    QLabel      *m_legendLow;
    QLabel      *m_legendHigh;
    QTreeWidget *m_seamList;
    QPushButton *m_findSeamsButton;
    QPushButton *m_addSeamButton;
    QPushButton *m_removeSeamButton;
    QPushButton *m_sewByClickButton;
    QCheckBox   *m_showSeamsCheck;
    QVector<GarmentPanel>    m_seamPanels{};  ///< panels the seams were last found on
    QVector<SeamSpec>        m_foundSeams{};
    QVector<SeamSpec>        m_addedSeams{};
    QSet<QString>            m_switchedOff{};
    QVector<QPair<int, int>> m_highlight{};
    bool                     m_updatingSeams{false};
    QTimer      *m_followTimer;
    QTimer      *m_saveTimer;
    bool         m_updatingList{false};
    bool         m_applyingSetup{false};
};

} // namespace Seamly3D

#endif // SEAMLY3D_PATTERNPIECESPANEL_H
