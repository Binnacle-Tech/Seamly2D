/******************************************************************************
 *   @file   viewport3d.cpp
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

#include "viewport3d.h"

#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLShaderProgram>
#include <QWheelEvent>
#include <QtMath>

#include <cmath>

namespace
{

// GLSL 1.10 / GLSL ES 1.00 compatible. QOpenGLShaderProgram defines the precision qualifiers away on desktop GL.
const char *const meshVertexShader =
    "attribute highp vec3 position;\n"
    "attribute highp vec3 normal;\n"
    "uniform highp mat4 mvp;\n"
    "uniform highp mat3 normalMatrix;\n"
    "varying highp vec3 viewNormal;\n"
    "void main()\n"
    "{\n"
    "    viewNormal = normalMatrix * normal;\n"
    "    gl_Position = mvp * vec4(position, 1.0);\n"
    "}\n";

const char *const meshFragmentShader =
    "uniform lowp vec4 color;\n"
    "varying highp vec3 viewNormal;\n"
    "void main()\n"
    "{\n"
    "    // Headlight from slightly above the camera; abs() lights back faces of open meshes too.\n"
    "    highp vec3 lightDirection = normalize(vec3(0.3, 0.6, 1.0));\n"
    "    highp float diffuse = abs(dot(normalize(viewNormal), lightDirection));\n"
    "    gl_FragColor = vec4(color.rgb * (0.3 + 0.7 * diffuse), color.a);\n"
    "}\n";

const char *const lineVertexShader =
    "attribute highp vec3 position;\n"
    "uniform highp mat4 mvp;\n"
    "void main()\n"
    "{\n"
    "    gl_Position = mvp * vec4(position, 1.0);\n"
    "}\n";

const char *const lineFragmentShader =
    "uniform lowp vec4 color;\n"
    "void main()\n"
    "{\n"
    "    gl_FragColor = color;\n"
    "}\n";

const int gridHalfLineCount = 10;

} // anonymous namespace

namespace Seamly3D
{

//---------------------------------------------------------------------------------------------------------------------
Viewport3D::Viewport3D(QWidget *parent)
    : QOpenGLWidget(parent)
    , m_meshVertices(QOpenGLBuffer::VertexBuffer)
    , m_meshIndices(QOpenGLBuffer::IndexBuffer)
    , m_gridVertices(QOpenGLBuffer::VertexBuffer)
{
    setFocusPolicy(Qt::StrongFocus);
}

//---------------------------------------------------------------------------------------------------------------------
Viewport3D::~Viewport3D()
{
    cleanupGL();
}

//---------------------------------------------------------------------------------------------------------------------
void Viewport3D::setMesh(const Mesh &mesh)
{
    m_mesh = mesh;
    if (m_mesh.normals.size() != m_mesh.positions.size())
    {
        m_mesh.computeNormals();
    }
    m_meshDirty = true;
    frameAll();
}

//---------------------------------------------------------------------------------------------------------------------
void Viewport3D::clearMesh()
{
    m_mesh.clear();
    m_meshDirty = true;
    update();
}

//---------------------------------------------------------------------------------------------------------------------
const Mesh &Viewport3D::mesh() const
{
    return m_mesh;
}

//---------------------------------------------------------------------------------------------------------------------
bool Viewport3D::isGridVisible() const
{
    return m_gridVisible;
}

//---------------------------------------------------------------------------------------------------------------------
OrbitCamera &Viewport3D::camera()
{
    return m_camera;
}

//---------------------------------------------------------------------------------------------------------------------
const OrbitCamera &Viewport3D::camera() const
{
    return m_camera;
}

//---------------------------------------------------------------------------------------------------------------------
QSize Viewport3D::minimumSizeHint() const
{
    return QSize(200, 200);
}

//---------------------------------------------------------------------------------------------------------------------
QSize Viewport3D::sizeHint() const
{
    return QSize(800, 600);
}

//---------------------------------------------------------------------------------------------------------------------
void Viewport3D::setGridVisible(bool visible)
{
    m_gridVisible = visible;
    update();
}

//---------------------------------------------------------------------------------------------------------------------
void Viewport3D::resetView()
{
    m_camera.reset();
    frameAll();
}

//---------------------------------------------------------------------------------------------------------------------
void Viewport3D::frameAll()
{
    m_camera.frame(m_mesh.bounds());
    update();
}

//---------------------------------------------------------------------------------------------------------------------
void Viewport3D::initializeGL()
{
    initializeOpenGLFunctions();

    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &Viewport3D::cleanupGL, Qt::UniqueConnection);

    m_meshProgram.reset(new QOpenGLShaderProgram);
    m_meshProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, meshVertexShader);
    m_meshProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, meshFragmentShader);
    m_meshProgram->bindAttributeLocation("position", 0);
    m_meshProgram->bindAttributeLocation("normal", 1);
    if (!m_meshProgram->link())
    {
        qWarning("Seamly3D: mesh shader failed to link: %s", qPrintable(m_meshProgram->log()));
    }

    m_lineProgram.reset(new QOpenGLShaderProgram);
    m_lineProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, lineVertexShader);
    m_lineProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, lineFragmentShader);
    m_lineProgram->bindAttributeLocation("position", 0);
    if (!m_lineProgram->link())
    {
        qWarning("Seamly3D: line shader failed to link: %s", qPrintable(m_lineProgram->log()));
    }

    m_meshVertices.create();
    m_meshIndices.create();
    m_gridVertices.create();
    m_meshDirty = true;
}

//---------------------------------------------------------------------------------------------------------------------
void Viewport3D::resizeGL(int width, int height)
{
    Q_UNUSED(width)
    Q_UNUSED(height)
    // The projection is recalculated from the widget size on every paint.
}

//---------------------------------------------------------------------------------------------------------------------
void Viewport3D::paintGL()
{
    if (m_meshDirty)
    {
        uploadMesh();
        uploadGrid();
        m_meshDirty = false;
    }

    const QColor background = palette().color(QPalette::Base);
    glClearColor(background.redF(), background.greenF(), background.blueF(), 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    const float aspect = height() > 0 ? static_cast<float>(width()) / static_cast<float>(height()) : 1.0f;
    const QMatrix4x4 view = m_camera.viewMatrix();
    const QMatrix4x4 mvp = m_camera.projectionMatrix(aspect) * view;

    if (m_gridVisible && m_gridVertexCount > 0 && m_lineProgram->isLinked())
    {
        m_lineProgram->bind();
        m_lineProgram->setUniformValue("mvp", mvp);
        m_lineProgram->setUniformValue("color", m_gridColor);
        m_gridVertices.bind();
        m_lineProgram->enableAttributeArray(0);
        m_lineProgram->setAttributeBuffer(0, GL_FLOAT, 0, 3, 3 * sizeof(float));
        glDrawArrays(GL_LINES, 0, m_gridVertexCount);
        m_lineProgram->disableAttributeArray(0);
        m_gridVertices.release();
        m_lineProgram->release();
    }

    if (m_meshIndexCount > 0 && m_meshProgram->isLinked())
    {
        m_meshProgram->bind();
        m_meshProgram->setUniformValue("mvp", mvp);
        m_meshProgram->setUniformValue("normalMatrix", view.normalMatrix());
        m_meshProgram->setUniformValue("color", m_meshColor);
        m_meshVertices.bind();
        m_meshIndices.bind();
        m_meshProgram->enableAttributeArray(0);
        m_meshProgram->enableAttributeArray(1);
        m_meshProgram->setAttributeBuffer(0, GL_FLOAT, 0, 3, 6 * sizeof(float));
        m_meshProgram->setAttributeBuffer(1, GL_FLOAT, 3 * sizeof(float), 3, 6 * sizeof(float));
        glDrawElements(GL_TRIANGLES, m_meshIndexCount, GL_UNSIGNED_INT, nullptr);
        m_meshProgram->disableAttributeArray(1);
        m_meshProgram->disableAttributeArray(0);
        m_meshIndices.release();
        m_meshVertices.release();
        m_meshProgram->release();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void Viewport3D::mousePressEvent(QMouseEvent *event)
{
    m_lastMousePos = event->position().toPoint();

    if (event->button() == Qt::LeftButton && !(event->modifiers() & Qt::ShiftModifier))
    {
        m_dragMode = DragMode::Orbit;
    }
    else if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton
             || event->button() == Qt::MiddleButton)
    {
        m_dragMode = DragMode::Pan;
    }
    event->accept();
}

//---------------------------------------------------------------------------------------------------------------------
void Viewport3D::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();
    const QPoint delta = pos - m_lastMousePos;
    m_lastMousePos = pos;

    switch (m_dragMode)
    {
        case DragMode::Orbit:
            m_camera.orbit(-0.4f * static_cast<float>(delta.x()), 0.4f * static_cast<float>(delta.y()));
            update();
            break;
        case DragMode::Pan:
            if (height() > 0)
            {
                const auto h = static_cast<float>(height());
                m_camera.pan(static_cast<float>(delta.x()) / h, static_cast<float>(delta.y()) / h);
                update();
            }
            break;
        case DragMode::None:
        default:
            break;
    }
    event->accept();
}

//---------------------------------------------------------------------------------------------------------------------
void Viewport3D::mouseReleaseEvent(QMouseEvent *event)
{
    m_dragMode = DragMode::None;
    event->accept();
}

//---------------------------------------------------------------------------------------------------------------------
void Viewport3D::wheelEvent(QWheelEvent *event)
{
    const int steps = event->angleDelta().y();
    if (steps != 0)
    {
        // One standard wheel notch (120) zooms by ~12%.
        m_camera.zoom(qPow(0.88, steps / 120.0));
        update();
    }
    event->accept();
}

//---------------------------------------------------------------------------------------------------------------------
void Viewport3D::cleanupGL()
{
    if (!m_meshVertices.isCreated() && !m_meshProgram)
    {
        return;
    }

    makeCurrent();
    m_meshVertices.destroy();
    m_meshIndices.destroy();
    m_gridVertices.destroy();
    m_meshProgram.reset();
    m_lineProgram.reset();
    doneCurrent();
}

//---------------------------------------------------------------------------------------------------------------------
void Viewport3D::uploadMesh()
{
    const QVector<float> vertexData = m_mesh.isEmpty() ? QVector<float>() : m_mesh.interleavedVertexData();
    m_meshVertices.bind();
    m_meshVertices.allocate(vertexData.constData(), static_cast<int>(vertexData.size() * sizeof(float)));
    m_meshVertices.release();

    const QVector<quint32> &indices = m_mesh.isEmpty() ? QVector<quint32>() : m_mesh.indices;
    m_meshIndices.bind();
    m_meshIndices.allocate(indices.constData(), static_cast<int>(indices.size() * sizeof(quint32)));
    m_meshIndices.release();

    m_meshIndexCount = static_cast<int>(indices.size());
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief uploadGrid builds a square grid on the XZ plane under the mesh, sized to the mesh.
 */
void Viewport3D::uploadGrid()
{
    const Bounds3D bounds = m_mesh.bounds();
    const float step = gridStep();
    const float floor = bounds.valid ? bounds.minimum.y() : 0.0f;
    const float centerX = bounds.valid ? std::round(bounds.center().x() / step) * step : 0.0f;
    const float centerZ = bounds.valid ? std::round(bounds.center().z() / step) * step : 0.0f;
    const float extent = step * gridHalfLineCount;

    QVector<float> lines;
    lines.reserve((2 * gridHalfLineCount + 1) * 2 * 2 * 3);
    for (int i = -gridHalfLineCount; i <= gridHalfLineCount; ++i)
    {
        const float offset = static_cast<float>(i) * step;
        lines << centerX + offset << floor << centerZ - extent << centerX + offset << floor << centerZ + extent;
        lines << centerX - extent << floor << centerZ + offset << centerX + extent << floor << centerZ + offset;
    }

    m_gridVertices.bind();
    m_gridVertices.allocate(lines.constData(), static_cast<int>(lines.size() * sizeof(float)));
    m_gridVertices.release();
    m_gridVertexCount = static_cast<int>(lines.size() / 3);
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief gridStep picks a power of ten spacing so the grid is a few times larger than the mesh in any units.
 */
float Viewport3D::gridStep() const
{
    const Bounds3D bounds = m_mesh.bounds();
    const float size = bounds.valid ? qMax(bounds.size().x(), bounds.size().z()) : 0.0f;
    if (size <= 0.0f)
    {
        return 1.0f;
    }
    return qPow(10.0f, std::floor(std::log10(size / 2.0f)));
}

} // namespace Seamly3D
