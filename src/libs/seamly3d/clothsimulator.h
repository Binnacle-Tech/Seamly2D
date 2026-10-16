/******************************************************************************
 *   @file   clothsimulator.h
 **  @author Seamly2D project
 **  @date   16 10, 2026
 **
 **  @brief  Drapes a sewn garment on the body.
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

#ifndef SEAMLY3D_CLOTHSIMULATOR_H
#define SEAMLY3D_CLOTHSIMULATOR_H

#include <QCoreApplication>
#include <QVector>
#include <QVector3D>

#include "garment.h"
#include "mesh.h"

namespace Seamly3D
{

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief The body as the cloth feels it: finds the closest point of the body surface near a point.
 *
 * The triangles are sorted into a uniform grid; each cell lists the triangles within reach of it, so a query looks at
 * one cell only.
 */
class BodyCollider
{
public:
    void build(const Mesh &body, float reach, float cellSize = 2.0f);
    void clear();
    bool isEmpty() const;
    float reach() const;

    /**
     * @brief closest finds the closest point of the body to @p point, if one is within reach.
     * @param surface receives the point on the body
     * @param normal receives the body's outward normal there
     * @return the signed distance to the body (negative inside), or the reach when the body is not that close
     */
    float closest(const QVector3D &point, QVector3D *surface = nullptr, QVector3D *normal = nullptr) const;

private:
    struct Triangle
    {
        QVector3D a, b, c;    // corners
        QVector3D na, nb, nc; // vertex normals, outward
        QVector3D face;       // face normal, outward
    };

    int cellIndex(int x, int y, int z) const;

    QVector<Triangle> m_triangles{};
    QVector<int>      m_cellStart{}; ///< Triangles of cell i are m_cellTriangles[m_cellStart[i]..m_cellStart[i+1])
    QVector<int>      m_cellTriangles{};
    QVector3D         m_origin{};
    int               m_size[3]{0, 0, 0};
    float             m_cellSize{2.0f};
    float             m_reach{0};
};

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief How the fabric behaves. Lengths in centimeters, time in seconds.
 */
struct ClothSettings
{
    float frameTime{1.0f / 30.0f};
    int   substeps{10};             ///< Solver steps per frame; more is stiffer and slower
    int   iterations{4};            ///< Passes over the stretch and seam constraints in each step
    float gravity{981.0f};          ///< cm/s²
    float stretchCompliance{0.0f};  ///< 0: the fabric does not stretch
    float bendCompliance{4e-4f};    ///< larger is softer: chiffon drapes, denim does not
    float damping{2.0f};            ///< 1/s
    float thickness{0.4f};          ///< How far the cloth keeps from the body
    float friction{0.3f};           ///< 0 slides freely, 1 sticks where it touches
    float sewingSpeed{30.0f};       ///< cm/s the seams pull together at
    float sewingGravity{0.0f};      ///< Gravity, as a share of the full gravity, until the seams are closed
    float weight{1.0f};             ///< Areal weight, relative to a 120 g/m² shirting
};

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief A kind of fabric: how it stretches, bends, weighs and slides.
 */
struct FabricPreset
{
    QString id{};
    QString name{};
    int     gramsPerSquareMeter{120};
    float   stretchCompliance{0};   ///< 0: does not stretch (wovens); knits stretch
    float   bendCompliance{4e-4f};  ///< larger drapes softer
    float   friction{0.3f};
    float   thickness{0.4f};        ///< centimeters the cloth keeps from the body
    float   damping{2.0f};

    /** @brief apply returns @p settings with this fabric's properties. */
    ClothSettings apply(ClothSettings settings) const;

    /** @brief The fabrics to choose from, shirting cotton first. */
    static QVector<FabricPreset> presets();
    static FabricPreset preset(const QString &id);
};

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Sews the garment panels together and drapes them on the body.
 *
 * Extended position based dynamics (XPBD) with small steps: every mesh edge keeps its length on the flat piece
 * (stretch), every pair of triangles resists folding (bend), seams pull their two edges together, and the body and the
 * floor push the cloth out. Panels that are not sewable stay where they are.
 */
class ClothSimulator
{
public:
    void setSettings(const ClothSettings &settings);
    const ClothSettings &settings() const;

    /** @brief setBody sets the body the garment drapes on. Its normals may point either way. */
    void setBody(const Mesh &body);
    void setGarment(const QVector<GarmentPanel> &panels, const QVector<Seam> &seams);
    void clear();

    bool  isEmpty() const;
    int   particleCount() const;
    int   seamCount() const;
    float time() const;

    /** @brief step advances the simulation by one frame. */
    void  step();

    /** @brief panels returns the garment panels with their shapes as they are now. */
    QVector<GarmentPanel> panels() const;

    float maxSeamGap() const;   ///< centimeters between the two sides of the widest open seam
    float maxSpeed() const;     ///< cm/s of the fastest particle in the last frame
    float averageSpeed() const; ///< cm/s the particles moved at on average in the last frame
    float maxPenetration() const; ///< centimeters the deepest particle is inside the body (0 when none is)

    /**
     * @brief strainMap is, for each particle of each panel, how much the fabric around it is stretched: the edges
     * meeting there, longer than on the flat piece by this share on average (0.02 is 2%; negative is compressed).
     */
    QVector<QVector<float>> strainMap() const;

    /**
     * @brief easeMap is, for each particle of each panel, how far the cloth is from the body, in centimeters: 0
     * where it lies on the body, up to @p limit.
     */
    QVector<QVector<float>> easeMap(float limit = 6.0f) const;
    bool  isSewn() const;       ///< all seams have been pulled closed

    static constexpr float maxStep = 1.0f; ///< centimeters a particle may move in one solver step

private:
    struct Distance
    {
        int   a;
        int   b;
        float rest;
    };

    /** @brief Particle a held to the point (1 - t) b0 + t b1 of the other side of a seam. */
    struct SeamLink
    {
        int   a;
        int   b0;
        int   b1;
        float t;
        float start; ///< the gap when sewing started
    };

    void substep(float h, float gravity, float sewingTarget);
    void solveDistances(const QVector<Distance> &constraints, float compliance, float h);
    void solveSeams(float target);
    void collide();
    void addSeam(const Seam &seam);
    QVector3D seamPoint(const SeamLink &link) const;

    ClothSettings         m_settings{};
    BodyCollider          m_body{};
    Mesh                  m_bodyMesh{};
    mutable BodyCollider  m_easeBody{};   ///< reaches further, for the ease map (built when first needed)
    mutable float         m_easeLimit{0};
    QVector<GarmentPanel> m_panels{};
    QVector<int>          m_offset{};    ///< First particle of each panel
    QVector<QVector3D>    m_position{};
    QVector<QVector3D>    m_previous{};
    QVector<QVector3D>    m_velocity{};
    QVector<float>        m_inverseMass{};
    QVector<Distance>     m_stretch{};
    QVector<Distance>     m_bend{};
    QVector<SeamLink>     m_seams{};
    int                   m_seamCount{0};
    float                 m_sewingTime{0};
    float                 m_time{0};
    float                 m_maxSpeed{0};
    float                 m_averageSpeed{0};
    float                 m_floor{0};
};

} // namespace Seamly3D

#endif // SEAMLY3D_CLOTHSIMULATOR_H
