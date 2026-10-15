/******************************************************************************
 *   @file   viewport3d.h
 **  @author Seamly2D project
 **  @date   15 10, 2026
 **
 **  @brief  OpenGL viewport of the 3D workspace.
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

#ifndef SEAMLY3D_VIEWPORT3D_H
#define SEAMLY3D_VIEWPORT3D_H

#include <QColor>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QPoint>
#include <QScopedPointer>

#include "mesh.h"
#include "orbitcamera.h"

class QOpenGLShaderProgram;

namespace Seamly3D
{

/**
 * @brief OpenGL viewport of the 3D workspace.
 *
 * Renders a shaded mesh (avatar) over a ground grid. Uses only QtGui/QtOpenGL with GLSL that runs on OpenGL 2.1 and
 * OpenGL ES 2.0 so it works with the default surface format on every platform Seamly2D supports.
 *
 * Mouse: left drag orbits, right or middle drag (or Shift + left drag) pans, the wheel zooms.
 */
class Viewport3D : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit Viewport3D(QWidget *parent = nullptr);
    ~Viewport3D() override;

    void        setMesh(const Mesh &mesh);
    void        clearMesh();
    const Mesh &mesh() const;

    bool        isGridVisible() const;

    OrbitCamera       &camera();
    const OrbitCamera &camera() const;

    QSize       minimumSizeHint() const override;
    QSize       sizeHint() const override;

public slots:
    void        setGridVisible(bool visible);
    void        resetView();
    void        frameAll();

protected:
    void        initializeGL() override;
    void        resizeGL(int width, int height) override;
    void        paintGL() override;

    void        mousePressEvent(QMouseEvent *event) override;
    void        mouseMoveEvent(QMouseEvent *event) override;
    void        mouseReleaseEvent(QMouseEvent *event) override;
    void        wheelEvent(QWheelEvent *event) override;

private:
    Q_DISABLE_COPY_MOVE(Viewport3D)

    enum class DragMode : char { None, Orbit, Pan };

    void        cleanupGL();
    void        uploadMesh();
    void        uploadGrid();
    float       gridStep() const;

    Mesh        m_mesh{};
    OrbitCamera m_camera{};

    QScopedPointer<QOpenGLShaderProgram> m_meshProgram;
    QScopedPointer<QOpenGLShaderProgram> m_lineProgram;
    QOpenGLBuffer m_meshVertices;
    QOpenGLBuffer m_meshIndices;
    QOpenGLBuffer m_gridVertices;
    int           m_meshIndexCount{0};
    int           m_gridVertexCount{0};
    bool          m_meshDirty{true};

    bool        m_gridVisible{true};
    QColor      m_meshColor{204, 186, 168};
    QColor      m_gridColor{140, 140, 140};

    DragMode    m_dragMode{DragMode::None};
    QPoint      m_lastMousePos{};
};

} // namespace Seamly3D

#endif // SEAMLY3D_VIEWPORT3D_H
