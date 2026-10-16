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

#include <limits>

#include <cmath>

namespace
{

// GLSL 1.10 / GLSL ES 1.00 compatible. QOpenGLShaderProgram defines the precision qualifiers away on desktop GL.
const char *const meshVertexShader =
    "attribute highp vec3 position;\n"
    "attribute highp vec3 normal;\n"
    "attribute lowp vec3 vertexColor;\n"
    "uniform highp mat4 mvp;\n"
    "uniform highp mat3 normalMatrix;\n"
    "uniform lowp vec4 color;\n"
    "uniform lowp float useVertexColor;\n"
    "varying highp vec3 viewNormal;\n"
    "varying lowp vec3 baseColor;\n"
    "void main()\n"
    "{\n"
    "    viewNormal = normalMatrix * normal;\n"
    "    baseColor = mix(color.rgb, vertexColor, useVertexColor);\n"
    "    gl_Position = mvp * vec4(position, 1.0);\n"
    "}\n";

const char *const meshFragmentShader =
    "uniform lowp vec4 color;\n"
    "varying highp vec3 viewNormal;\n"
    "varying lowp vec3 baseColor;\n"
    "void main()\n"
    "{\n"
    "    // Headlight from slightly above the camera; abs() lights back faces of open meshes too.\n"
    "    highp vec3 lightDirection = normalize(vec3(0.3, 0.6, 1.0));\n"
    "    highp float diffuse = abs(dot(normalize(viewNormal), lightDirection));\n"
    "    gl_FragColor = vec4(baseColor * (0.3 + 0.7 * diffuse), color.a);\n"
    "}\n";

const char *const lineVertexShader =
    "attribute highp vec3 position;\n"
    "uniform highp mat4 mvp;\n"
    "uniform highp vec3 eye;\n"
    "uniform highp float lift;\n"
    "void main()\n"
    "{\n"
    "    gl_Position = mvp * vec4(position + normalize(eye - position) * lift, 1.0);\n"
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
    , m_lineVertices(QOpenGLBuffer::VertexBuffer)
{
    setFocusPolicy(Qt::StrongFocus);
}

//---------------------------------------------------------------------------------------------------------------------
Viewport3D::~Viewport3D()
{
    // QOpenGLWidget destroys the context after our members are gone; it must not call back into cleanupGL() then.
    if (context() != nullptr)
    {
        disconnect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &Viewport3D::cleanupGL);
    }
    cleanupGL();
}

//---------------------------------------------------------------------------------------------------------------------
void Viewport3D::setMesh(const Mesh &mesh, bool frame)
{
    m_mesh = mesh;
    if (m_mesh.normals.size() != m_mesh.positions.size())
    {
        m_mesh.computeNormals();
    }
    m_meshDirty = true;
    if (frame)
    {
        frameAll();
    }
    else
    {
        update();
    }
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
void Viewport3D::setOverlays(const QVector<Overlay> &overlays)
{
    m_overlays = overlays;
    for (Overlay &overlay : m_overlays)
    {
        if (overlay.mesh.normals.size() != overlay.mesh.positions.size())
        {
            overlay.mesh.computeNormals();
        }
    }
    m_overlaysDirty = true;
    update();
}

//---------------------------------------------------------------------------------------------------------------------
void Viewport3D::setLines(const QVector<LineSet> &lines)
{
    m_lines = lines;
    m_linesDirty = true;
    update();
}

//---------------------------------------------------------------------------------------------------------------------
const QVector<Viewport3D::LineSet> &Viewport3D::lines() const
{
    return m_lines;
}

//---------------------------------------------------------------------------------------------------------------------
const QVector<Viewport3D::Overlay> &Viewport3D::overlays() const
{
    return m_overlays;
}

//---------------------------------------------------------------------------------------------------------------------
Bounds3D Viewport3D::sceneBounds() const
{
    Bounds3D bounds = m_mesh.bounds();
    for (const Overlay &overlay : m_overlays)
    {
        const Bounds3D overlayBounds = overlay.mesh.bounds();
        if (overlayBounds.valid)
        {
            bounds.expand(overlayBounds.minimum);
            bounds.expand(overlayBounds.maximum);
        }
    }
    return bounds;
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
    m_camera.frame(sceneBounds());
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
    m_meshProgram->bindAttributeLocation("vertexColor", 2);
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
    m_lineVertices.create();
    m_linesDirty = true;
    m_meshDirty = true;
    m_overlaysDirty = true;
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
    if (m_overlaysDirty)
    {
        uploadOverlays();
        m_overlaysDirty = false;
    }
    if (m_linesDirty)
    {
        // All line sets in one buffer.
        QVector<float> data;
        m_lineRanges.clear();
        for (const LineSet &set : qAsConst(m_lines))
        {
            m_lineRanges.append(qMakePair(static_cast<int>(data.size() / 3), static_cast<int>(set.segments.size())));
            for (const QVector3D &p : set.segments)
            {
                data << p.x() << p.y() << p.z();
            }
        }
        m_lineVertices.bind();
        m_lineVertices.allocate(data.constData(), static_cast<int>(data.size() * sizeof(float)));
        m_lineVertices.release();
        m_linesDirty = false;
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
        m_lineProgram->setUniformValue("eye", m_camera.eye());
        m_lineProgram->setUniformValue("lift", 0.0f);
        m_gridVertices.bind();
        m_lineProgram->enableAttributeArray(0);
        m_lineProgram->setAttributeBuffer(0, GL_FLOAT, 0, 3, 3 * sizeof(float));
        glDrawArrays(GL_LINES, 0, m_gridVertexCount);
        m_lineProgram->disableAttributeArray(0);
        m_gridVertices.release();
        m_lineProgram->release();
    }

    if (m_meshProgram->isLinked())
    {
        drawMesh(mvp, view, m_meshVertices, m_meshIndices, m_meshIndexCount, m_meshColor, nullptr);
        for (int i = 0; i < m_overlayBuffers.size() && i < m_overlays.size(); ++i)
        {
            GpuMesh &gpu = m_overlayBuffers[i];
            drawMesh(mvp, view, gpu.vertices, gpu.indices, gpu.indexCount, m_overlays.at(i).color,
                     gpu.hasColors ? &gpu.colors : nullptr);
        }
    }

    // Lines on the cloth: lifted toward the eye by a centimeter, so the cloth they lie on does not hide them.
    if (!m_lineRanges.isEmpty() && m_lineProgram->isLinked())
    {
        m_lineProgram->bind();
        m_lineProgram->setUniformValue("mvp", mvp);
        m_lineProgram->setUniformValue("eye", m_camera.eye());
        m_lineProgram->setUniformValue("lift", 1.0f);
        m_lineVertices.bind();
        m_lineProgram->enableAttributeArray(0);
        m_lineProgram->setAttributeBuffer(0, GL_FLOAT, 0, 3, 3 * sizeof(float));
        for (int i = 0; i < m_lineRanges.size() && i < m_lines.size(); ++i)
        {
            if (m_lineRanges.at(i).second < 2)
            {
                continue;
            }
            if (m_lines.at(i).onTop)
            {
                glDisable(GL_DEPTH_TEST);
            }
            else
            {
                glEnable(GL_DEPTH_TEST);
            }
            m_lineProgram->setUniformValue("color", m_lines.at(i).color);
            glLineWidth(m_lines.at(i).width);
            glDrawArrays(GL_LINES, m_lineRanges.at(i).first, m_lineRanges.at(i).second);
        }
        glLineWidth(1.0f);
        m_lineProgram->disableAttributeArray(0);
        m_lineVertices.release();
        m_lineProgram->release();
        glEnable(GL_DEPTH_TEST);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void Viewport3D::drawMesh(const QMatrix4x4 &mvp, const QMatrix4x4 &view, QOpenGLBuffer &vertices,
                          QOpenGLBuffer &indices, int indexCount, const QColor &color, QOpenGLBuffer *colors)
{
    if (indexCount <= 0)
    {
        return;
    }

    m_meshProgram->bind();
    m_meshProgram->setUniformValue("mvp", mvp);
    m_meshProgram->setUniformValue("normalMatrix", view.normalMatrix());
    m_meshProgram->setUniformValue("color", color);
    m_meshProgram->setUniformValue("useVertexColor", colors != nullptr ? 1.0f : 0.0f);
    if (colors != nullptr)
    {
        colors->bind();
        m_meshProgram->enableAttributeArray(2);
        m_meshProgram->setAttributeBuffer(2, GL_FLOAT, 0, 3, 3 * sizeof(float));
        colors->release();
    }
    else
    {
        m_meshProgram->disableAttributeArray(2);
        m_meshProgram->setAttributeValue(2, 0.0f, 0.0f, 0.0f);
    }
    vertices.bind();
    indices.bind();
    m_meshProgram->enableAttributeArray(0);
    m_meshProgram->enableAttributeArray(1);
    m_meshProgram->setAttributeBuffer(0, GL_FLOAT, 0, 3, 6 * sizeof(float));
    m_meshProgram->setAttributeBuffer(1, GL_FLOAT, 3 * sizeof(float), 3, 6 * sizeof(float));
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    m_meshProgram->disableAttributeArray(2);
    m_meshProgram->disableAttributeArray(1);
    m_meshProgram->disableAttributeArray(0);
    indices.release();
    vertices.release();
    m_meshProgram->release();
}

//---------------------------------------------------------------------------------------------------------------------
void Viewport3D::uploadOverlays()
{
    destroyOverlayBuffers();
    m_overlayBuffers.resize(m_overlays.size());
    for (int i = 0; i < m_overlays.size(); ++i)
    {
        const Mesh &mesh = m_overlays.at(i).mesh;
        GpuMesh &gpu = m_overlayBuffers[i];
        gpu.vertices.create();
        gpu.indices.create();

        const QVector<float> vertexData = mesh.interleavedVertexData();
        gpu.vertices.bind();
        gpu.vertices.allocate(vertexData.constData(), static_cast<int>(vertexData.size() * sizeof(float)));
        gpu.vertices.release();

        gpu.indices.bind();
        gpu.indices.allocate(mesh.indices.constData(), static_cast<int>(mesh.indices.size() * sizeof(quint32)));
        gpu.indices.release();
        gpu.indexCount = static_cast<int>(mesh.indices.size());

        // Colors per vertex (fit maps), when there is one for every vertex.
        const QVector<QColor> &colors = m_overlays.at(i).vertexColors;
        gpu.hasColors = !colors.isEmpty() && colors.size() == mesh.positions.size();
        if (gpu.hasColors)
        {
            QVector<float> colorData;
            colorData.reserve(colors.size() * 3);
            for (const QColor &c : colors)
            {
                colorData << static_cast<float>(c.redF()) << static_cast<float>(c.greenF())
                          << static_cast<float>(c.blueF());
            }
            gpu.colors.create();
            gpu.colors.bind();
            gpu.colors.allocate(colorData.constData(), static_cast<int>(colorData.size() * sizeof(float)));
            gpu.colors.release();
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
void Viewport3D::destroyOverlayBuffers()
{
    for (GpuMesh &gpu : m_overlayBuffers)
    {
        gpu.vertices.destroy();
        gpu.indices.destroy();
        gpu.colors.destroy();
    }
    m_overlayBuffers.clear();
}

//---------------------------------------------------------------------------------------------------------------------
void Viewport3D::mousePressEvent(QMouseEvent *event)
{
    m_lastMousePos = event->position().toPoint();

    if (m_overlaysMovable && event->button() == Qt::LeftButton && (event->modifiers() & Qt::ControlModifier))
    {
        QVector3D hit;
        const int index = overlayAt(event->position(), &hit);
        if (index >= 0)
        {
            m_dragMode = DragMode::Overlay;
            m_dragOverlay = index;
            m_dragPoint = hit;
            emit overlayPressed(index);
            event->accept();
            return;
        }
    }
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
        case DragMode::Overlay:
            if (height() > 0)
            {
                // The mouse moved this far on the plane facing the camera through the point held.
                const QVector3D eye = m_camera.eye();
                const QVector3D forward = (m_camera.target() - eye).normalized();
                QVector3D right = QVector3D::crossProduct(forward, QVector3D(0, 1, 0));
                right = right.lengthSquared() > 1e-8f ? right.normalized() : QVector3D(1, 0, 0);
                const QVector3D up = QVector3D::crossProduct(right, forward).normalized();
                const float depth = QVector3D::dotProduct(m_dragPoint - eye, forward);
                const float perPixel = 2.0f * depth * qTan(qDegreesToRadians(m_camera.fieldOfView()) / 2)
                                       / static_cast<float>(height());
                const QVector3D moved = (right * static_cast<float>(delta.x())
                                         - up * static_cast<float>(delta.y())) * perPixel;
                emit overlayDragged(m_dragOverlay, m_dragPoint, moved);
                m_dragPoint += moved;
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
    m_dragOverlay = -1;
    event->accept();
}

//---------------------------------------------------------------------------------------------------------------------
void Viewport3D::setOverlaysMovable(bool movable)
{
    m_overlaysMovable = movable;
    if (!movable && m_dragMode == DragMode::Overlay)
    {
        m_dragMode = DragMode::None;
        m_dragOverlay = -1;
    }
}

//---------------------------------------------------------------------------------------------------------------------
bool Viewport3D::overlaysMovable() const
{
    return m_overlaysMovable;
}

//---------------------------------------------------------------------------------------------------------------------
int Viewport3D::overlayAt(const QPointF &position, QVector3D *hit) const
{
    if (width() <= 0 || height() <= 0)
    {
        return -1;
    }
    // From the point on the screen, straight into the scene.
    const float aspect = static_cast<float>(width()) / static_cast<float>(height());
    const QMatrix4x4 inverse = (m_camera.projectionMatrix(aspect) * m_camera.viewMatrix()).inverted();
    const auto x = static_cast<float>(2.0 * position.x() / width() - 1.0);
    const auto y = static_cast<float>(1.0 - 2.0 * position.y() / height());
    const QVector3D near = inverse.map(QVector3D(x, y, -1));
    const QVector3D far = inverse.map(QVector3D(x, y, 1));
    const QVector3D direction = (far - near).normalized();
    float distance = 0;
    const int index = pickOverlay(m_overlays, near, direction, &distance);
    if (index >= 0 && hit != nullptr)
    {
        *hit = near + direction * distance;
    }
    return index;
}

//---------------------------------------------------------------------------------------------------------------------
int Viewport3D::pickOverlay(const QVector<Overlay> &overlays, const QVector3D &origin, const QVector3D &direction,
                            float *distance)
{
    int nearest = -1;
    float best = std::numeric_limits<float>::max();
    for (int o = 0; o < overlays.size(); ++o)
    {
        const Mesh &mesh = overlays.at(o).mesh;
        for (int t = 0; t + 2 < mesh.indices.size(); t += 3)
        {
            // Moller-Trumbore, both sides of the triangle.
            const QVector3D &a = mesh.positions.at(static_cast<int>(mesh.indices.at(t)));
            const QVector3D ab = mesh.positions.at(static_cast<int>(mesh.indices.at(t + 1))) - a;
            const QVector3D ac = mesh.positions.at(static_cast<int>(mesh.indices.at(t + 2))) - a;
            const QVector3D p = QVector3D::crossProduct(direction, ac);
            const float determinant = QVector3D::dotProduct(ab, p);
            if (qAbs(determinant) < 1e-9f)
            {
                continue;
            }
            const float inverse = 1.0f / determinant;
            const QVector3D s = origin - a;
            const float u = QVector3D::dotProduct(s, p) * inverse;
            if (u < 0 || u > 1)
            {
                continue;
            }
            const QVector3D q = QVector3D::crossProduct(s, ab);
            const float v = QVector3D::dotProduct(direction, q) * inverse;
            if (v < 0 || u + v > 1)
            {
                continue;
            }
            const float along = QVector3D::dotProduct(ac, q) * inverse;
            if (along > 0 && along < best)
            {
                best = along;
                nearest = o;
            }
        }
    }
    if (nearest >= 0 && distance != nullptr)
    {
        *distance = best;
    }
    return nearest;
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
    m_lineVertices.destroy();
    destroyOverlayBuffers();
    m_overlaysDirty = true;
    m_linesDirty = true;
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
