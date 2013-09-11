// Copyright (C) 2012, Chris J. Foster and the other authors and contributors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of the software's owners nor the names of its
//   contributors may be used to endorse or promote products derived from this
//   software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// (This is the BSD 3-clause license)

#include "ptview.h"
#include "mainwindow.h"
#include "shader.h"

#include "config.h"
#include "glutil.h"
#include "util.h"

#include <QtCore/QTimer>
#include <QtCore/QTime>
#include <QtGui/QKeyEvent>
#include <QtGui/QLayout>
#include <QtOpenGL/QGLFramebufferObject>
//#include <QtOpenGL/QGLBuffer>

#include "ptview.h"
#include "mesh.h"
#include "tinyformat.h"


//----------------------------------------------------------------------
inline float rad2deg(float r)
{
    return r*180/M_PI;
}

template<typename T>
inline QVector3D exr2qt(const Imath::Vec3<T>& v)
{
    return QVector3D(v.x, v.y, v.z);
}

inline Imath::V3d qt2exr(const QVector3D& v)
{
    return Imath::V3d(v.x(), v.y(), v.z());
}

inline Imath::M44f qt2exr(const QMatrix4x4& m)
{
    Imath::M44f mOut;
    for(int j = 0; j < 4; ++j)
    for(int i = 0; i < 4; ++i)
        mOut[j][i] = m.constData()[4*j + i];
    return mOut;
}


static const size_t g_defaultPointRenderCount = 1000000;

//------------------------------------------------------------------------------
PointView::PointView(QWidget *parent)
    : QGLWidget(parent),
    m_camera(false, false),
    m_prevMousePos(0,0),
    m_mouseButton(Qt::NoButton),
    m_cursorPos(0),
    m_prevCursorSnap(0),
    m_drawOffset(0),
    m_backgroundColor(60, 50, 50),
    m_drawBoundingBoxes(true),
    m_drawPoints(true),
    m_drawMeshes(true),
    m_shaderProgram(),
    m_points(),
    m_shaderParamsUI(0),
    m_maxPointCount(100000000),
    m_incrementalFrameTimer(0),
    m_incrementalDraw(false),
    m_maxPointsPerFrame(g_defaultPointRenderCount)
{
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    m_camera.setClipFar(FLT_MAX);
    // Don't use signals/slots for handling view changes... this seems to
    // introduce a bit of extra lag for slowly drawing scenes (?)
    //connect(&m_camera, SIGNAL(projectionChanged()), this, SLOT(restartRender()));
    //connect(&m_camera, SIGNAL(viewChanged()), this, SLOT(restartRender()));

    makeCurrent();
    m_shaderProgram.reset(new ShaderProgram(context()));
    connect(m_shaderProgram.get(), SIGNAL(uniformValuesChanged()),
            this, SLOT(restartRender()));
    connect(m_shaderProgram.get(), SIGNAL(shaderChanged()),
            this, SLOT(restartRender()));
    connect(m_shaderProgram.get(), SIGNAL(paramsChanged()),
            this, SLOT(setupShaderParamUI()));

    m_incrementalFrameTimer = new QTimer(this);
    m_incrementalFrameTimer->setSingleShot(false);
    connect(m_incrementalFrameTimer, SIGNAL(timeout()), this, SLOT(updateGL()));
}

PointView::~PointView() { }


void PointView::restartRender()
{
    m_incrementalDraw = false;
    updateGL();
}


void PointView::loadPointFilesImpl(PointArrayVec& pointArrays,
                                   MeshVec& meshes,
                                   LineSegVec& lines,
                                   const QStringList& fileNames)
{
    emit fileLoadStarted();
    pointArrays.clear();
    size_t maxCount = m_maxPointCount / fileNames.size();
    QStringList successfullyLoaded;
    for(int i = 0; i < fileNames.size(); ++i)
    {
        const QString& fileName = fileNames[i];
        if(fileName.endsWith(".ply"))
        {
            // Load data from ply format
            std::unique_ptr<TriMesh> mesh;
            std::unique_ptr<LineSegments> lineSegs;
            if(readPlyFile(fileName, mesh, lineSegs))
            {
                if (mesh)
                    meshes.push_back(std::move(mesh));
                if (lineSegs)
                    lines.push_back(std::move(lineSegs));
            }
        }
        else
        {
            // Load point cloud
            std::unique_ptr<PointArray> points(new PointArray());
            connect(points.get(), SIGNAL(pointsLoaded(int)),
                    this, SIGNAL(pointsLoaded(int)));
            connect(points.get(), SIGNAL(loadStepStarted(QString)),
                    this, SIGNAL(loadStepStarted(QString)));
            if(points->loadPointFile(fileName, maxCount) && !points->empty())
            {
                pointArrays.push_back(std::move(points));
                successfullyLoaded.push_back(fileName);
                emit pointFilesLoaded(successfullyLoaded);
            }
        }
    }
    emit fileLoadFinished();
    setupShaderParamUI(); // may have changed file name list
    m_maxPointsPerFrame = g_defaultPointRenderCount;
}


void PointView::loadFiles(const QStringList& fileNames)
{
    m_meshes.clear(); // FIXME - reloadFiles should reload meshes too
    loadPointFilesImpl(m_points, m_meshes, m_lines, fileNames);
    if(!m_points.empty())
    {
        m_cursorPos = m_points[0]->centroid();
        m_drawOffset = m_points[0]->offset();
        m_camera.setCenter(exr2qt(m_cursorPos - m_drawOffset));
        double diag = (m_points[0]->boundingBox().max -
                       m_points[0]->boundingBox().min).length();
        m_camera.setEyeToCenterDistance(diag*0.7);
    }
    else if(!m_meshes.empty())
    {
        m_cursorPos = m_meshes[0]->centroid();
        m_drawOffset = m_meshes[0]->offset();
        m_camera.setCenter(exr2qt(m_cursorPos - m_drawOffset));
    }
    else if(!m_lines.empty())
    {
        m_cursorPos = m_lines[0]->centroid();
        m_drawOffset = m_lines[0]->offset();
        m_camera.setCenter(exr2qt(m_cursorPos - m_drawOffset));
    }
    restartRender();
}


void PointView::reloadFiles()
{
    m_drawOffset = m_points[0]->offset();
    QStringList fileNames;
    for(size_t i = 0; i < m_points.size(); ++i)
        fileNames << m_points[i]->fileName();
    loadPointFilesImpl(m_points, m_meshes, m_lines, fileNames);
    if(!m_points.empty())
    {
        m_drawOffset = m_points[0]->offset();
        m_camera.setCenter(exr2qt(m_cursorPos - m_drawOffset));
    }
    restartRender();
}


void PointView::setShaderParamsUIWidget(QWidget* widget)
{
    m_shaderParamsUI = widget;
}


void PointView::setupShaderParamUI()
{
    if (!m_shaderProgram || !m_shaderParamsUI)
        return;
    while (QWidget* child = m_shaderParamsUI->findChild<QWidget*>())
        delete child;
    delete m_shaderParamsUI->layout();
    QStringList fileNames;
    for(size_t i = 0; i < m_points.size(); ++i)
        fileNames << QFileInfo(m_points[i]->fileName()).fileName();
    m_shaderProgram->setupParameterUI(m_shaderParamsUI, fileNames);
}


void PointView::setBackground(QColor col)
{
    m_backgroundColor = col;
    restartRender();
}


void PointView::toggleDrawBoundingBoxes()
{
    m_drawBoundingBoxes = !m_drawBoundingBoxes;
    restartRender();
}

void PointView::toggleDrawPoints()
{
    m_drawPoints = !m_drawPoints;
    restartRender();
}

void PointView::toggleDrawMeshes()
{
    m_drawMeshes = !m_drawMeshes;
    restartRender();
}

void PointView::toggleCameraMode()
{
    m_camera.setTrackballInteraction(!m_camera.trackballInteraction());
}


void PointView::setStochasticSimplification(bool enabled)
{
    m_useStochasticSimplification = enabled;
    restartRender();
}


void PointView::setMaxPointCount(size_t maxPointCount)
{
    m_maxPointCount = maxPointCount;
}


void PointView::initializeGL()
{
    m_meshFaceShader.reset(new ShaderProgram(context()));
    m_meshFaceShader->setShaderFromSourceFile(
        QString(DISPLAZ_SHADER_BASE_PATH) + "/shaders/meshface.glsl");
    m_meshEdgeShader.reset(new ShaderProgram(context()));
    m_meshEdgeShader->setShaderFromSourceFile(
        QString(DISPLAZ_SHADER_BASE_PATH) + "/shaders/meshedge.glsl");
}


void PointView::resizeGL(int w, int h)
{
    // Draw on full window
    glViewport(0, 0, w, h);
    m_camera.setViewport(QRect(0,0,w,h));
    // TODO:
    // * Should we use multisampling 1 to avoid binding to a texture?
    const QGLFormat fmt = context()->format();
    QGLFramebufferObjectFormat fboFmt;
    fboFmt.setAttachment(QGLFramebufferObject::Depth);
    fboFmt.setSamples(fmt.samples());
    //fboFmt.setTextureTarget();
    m_incrementalFramebuffer.reset(
        new QGLFramebufferObject(w, h, fboFmt));
    restartRender();
}


void PointView::paintGL()
{
    QTime frameTimer;
    frameTimer.start();

    m_incrementalFramebuffer->bind();
    //--------------------------------------------------
    // Draw main scene
    // Set camera projection
    glMatrixMode(GL_PROJECTION);
    glLoadMatrix(qt2exr(m_camera.projectionMatrix()));
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrix(qt2exr(m_camera.viewMatrix()));

    glClearDepth(1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearColor(m_backgroundColor.redF(), m_backgroundColor.greenF(),
                 m_backgroundColor.blueF(), 1.0f);
    if (!m_incrementalDraw)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Draw meshes and lines
    if (m_drawMeshes && !m_meshes.empty() && !m_incrementalDraw)
    {
        for(size_t i = 0; i < m_meshes.size(); ++i)
            drawMesh(*m_meshes[i], m_drawOffset);
    }
    if (m_drawMeshes && !m_lines.empty() && !m_incrementalDraw)
    {
        QGLShaderProgram& meshEdgeShader = m_meshEdgeShader->shaderProgram();
        glLineWidth(1);
        meshEdgeShader.bind();
        for(size_t i = 0; i < m_lines.size(); ++i)
        {
            glPushMatrix();
            V3d offset = m_lines[i]->offset() - m_drawOffset;
            glTranslatef(offset.x, offset.y, offset.z);
            m_lines[i]->drawEdges(meshEdgeShader);
            glPopMatrix();
        }
        meshEdgeShader.release();
    }

    // Figure out how many points we should render
    size_t totPoints = 0;
    for (size_t i = 0; i < m_points.size(); ++i)
        totPoints += m_points[i]->size();
    size_t numPointsToRender = m_maxPointsPerFrame;
    bool simplify = numPointsToRender < totPoints && m_useStochasticSimplification;
    if (!simplify)
        numPointsToRender = totPoints;
    size_t totDrawn = 0;
    if (m_drawPoints)
    {
        // Render points
        totDrawn = drawPoints(m_points, numPointsToRender, simplify,
                              m_incrementalDraw);
    }

    // Measure frame time to update estimate for how many points we can
    // draw interactively
    glFinish();
    if (m_drawPoints && !m_incrementalDraw)
    {
        int frameTime = frameTimer.elapsed();
        // Aim for a frame time which is ok for desktop usage
        const double targetMillisecs = 50;
        double predictedPointNumber = numPointsToRender * targetMillisecs /
                                      std::max(1, frameTime);
        // Simple smoothing to avoid wild jumps in point count
        double decayFactor = 0.7;
        m_maxPointsPerFrame = (size_t) ( decayFactor     * m_maxPointsPerFrame +
                                        (1-decayFactor) * predictedPointNumber );
    }
    m_incrementalFramebuffer->release();
    QGLFramebufferObject::blitFramebuffer(0, QRect(0,0,width(),height()),
                                          m_incrementalFramebuffer.get(),
                                          QRect(0,0,width(),height()),
                                          GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Draw overlay stuff, including cursor position.
    drawCursor(m_cursorPos - m_drawOffset);

    // Set up timer to draw a high quality frame if necessary
    if (m_drawPoints && simplify)
    {
        if (totDrawn == 0)
            m_incrementalFrameTimer->stop();
        else
            m_incrementalFrameTimer->start(10);
        m_incrementalDraw = true;
    }
}


void PointView::drawMesh(const TriMesh& mesh, const V3d& drawOffset) const
{
    glPushMatrix();
    V3d offset = mesh.offset() - drawOffset;
    glTranslatef(offset.x, offset.y, offset.z);
    QGLShaderProgram& meshFaceShader = m_meshFaceShader->shaderProgram();
    meshFaceShader.bind();
    meshFaceShader.setUniformValue("lightDir_eye",
                m_camera.viewMatrix().mapVector(QVector3D(1,1,-1).normalized()));
    mesh.drawFaces(meshFaceShader);
    meshFaceShader.release();
    //glLineWidth(1);
    //QGLShaderProgram& meshEdgeShader = m_meshEdgeShader->shaderProgram();
    //meshEdgeShader.bind();
    //mesh.drawEdges(meshEdgeShader);
    //meshEdgeShader.release();
    glPopMatrix();
}


void PointView::mousePressEvent(QMouseEvent* event)
{
    m_mouseButton = event->button();
    m_prevMousePos = event->pos();
}


void PointView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MidButton)
    {
        QMatrix4x4 mat = m_camera.viewportMatrix()*m_camera.projectionMatrix()*m_camera.viewMatrix();
        double z = mat.map(exr2qt(m_cursorPos - m_drawOffset)).z();
        m_cursorPos = qt2exr(mat.inverted().map(QVector3D(event->x(), event->y(), z))) + m_drawOffset;
        snapCursorAndCentre(0.025);
    }
    restartRender();
}


void PointView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_mouseButton == Qt::MidButton)
        return;
    bool zooming = m_mouseButton == Qt::RightButton;
    if(event->modifiers() & Qt::ControlModifier)
    {
        m_cursorPos = qt2exr(
            m_camera.mouseMovePoint(exr2qt(m_cursorPos - m_drawOffset),
                                    event->pos() - m_prevMousePos,
                                    zooming) ) + m_drawOffset;
        restartRender();
    }
    else
    {
        m_camera.mouseDrag(m_prevMousePos, event->pos(), zooming);
        restartRender();
    }
    m_prevMousePos = event->pos();
}


void PointView::wheelEvent(QWheelEvent* event)
{
    // Translate mouse wheel events into vertical dragging for simplicity.
    m_camera.mouseDrag(QPoint(0,0), QPoint(0, -event->delta()/2), true);
    restartRender();
}


void PointView::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_C)
    {
        // Centre camera on current cursor location
        m_camera.setCenter(exr2qt(m_cursorPos - m_drawOffset));
        restartRender();
    }
    else if(event->key() == Qt::Key_S)
    {
        snapCursorAndCentre(1);
    }
    else
        event->ignore();
}


/// Draw the 3D cursor
void PointView::drawCursor(const V3f& cursorPos) const
{
    // Draw a point at the centre of the cursor.
    glColor3f(1,1,1);
    glPointSize(1);
    glBegin(GL_POINTS);
        glVertex(cursorPos);
    glEnd();

    // Find position of cursor in screen space
    V3f screenP3 = qt2exr(m_camera.projectionMatrix()*m_camera.viewMatrix() *
                          exr2qt(cursorPos));
    // Cull if behind the camera
    if((m_camera.viewMatrix() * exr2qt(cursorPos)).z() > 0)
        return;

    // Now draw a 2D overlay over the 3D scene to allow user to pinpoint the
    // cursor, even when when it's behind something.
    glPushAttrib(GL_ENABLE_BIT);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0,width(), 0,height(), 0, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);

    // Position in ortho coord system
    V2f p2 = 0.5f * V2f(width(), height()) *
             (V2f(screenP3.x, screenP3.y) + V2f(1.0f));
    float r = 10;
    glLineWidth(2);
    glColor3f(1,1,1);
    // Combined white and black crosshairs, so they can be seen on any
    // background.
    glTranslatef(p2.x, p2.y, 0);
    glBegin(GL_LINES);
        glVertex(V2f(r,   0));
        glVertex(V2f(2*r, 0));
        glVertex(-V2f(r,   0));
        glVertex(-V2f(2*r, 0));
        glVertex(V2f(0,   r));
        glVertex(V2f(0, 2*r));
        glVertex(-V2f(0,   r));
        glVertex(-V2f(0, 2*r));
    glEnd();
    glColor3f(0,0,0);
    glRotatef(45,0,0,1);
    glBegin(GL_LINES);
        glVertex(V2f(r,   0));
        glVertex(V2f(2*r, 0));
        glVertex(-V2f(r,   0));
        glVertex(-V2f(2*r, 0));
        glVertex(V2f(0,   r));
        glVertex(V2f(0, 2*r));
        glVertex(-V2f(0,   r));
        glVertex(-V2f(0, 2*r));
    glEnd();

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
}


/// Draw point cloud
size_t PointView::drawPoints(const PointArrayVec& allPoints,
                             size_t numPointsToRender, bool simplify,
                             bool incrementalDraw)
{
    if (allPoints.empty())
        return 0;
    for(size_t i = 0; i < allPoints.size(); ++i)
    {
        PointArray& points = *allPoints[i];
        if(points.empty())
            continue;
        if(m_drawBoundingBoxes && !incrementalDraw)
        {
            // Draw bounding box
            Imath::Box3f bbox;
            bbox.min = points.boundingBox().min - m_drawOffset;
            bbox.max = points.boundingBox().max - m_drawOffset;
            drawBoundingBox(bbox, Imath::C3f(1));
            //points.drawTree();
        }
    }
    glEnable(GL_POINT_SPRITE);
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    V3d globalCamPos = qt2exr(m_camera.position()) + m_drawOffset;
    double quality = 1;
    if (simplify)
    {
        // Get total number of points we would draw at quality == 1
        size_t totSimplified = 0;
        for(size_t i = 0; i < allPoints.size(); ++i)
            totSimplified += allPoints[i]->simplifiedSize(globalCamPos,
                                                         incrementalDraw);
        quality = double(numPointsToRender) / totSimplified;
    }
    // Draw points
    QGLShaderProgram& prog = m_shaderProgram->shaderProgram();
    prog.bind();
    m_shaderProgram->setUniforms();
    size_t totDrawn = 0;
    for(size_t i = 0; i < allPoints.size(); ++i)
    {
        PointArray& points = *allPoints[i];
        if(points.empty())
            continue;
        glPushMatrix();
        V3d offset = points.offset() - m_drawOffset;
        glTranslatef(offset.x, offset.y, offset.z);
        //prog.setUniformValue("modelViewMatrix", ); // TODO
        //prog.setUniformValue("projectionMatrix", );
        V3f relCursor = m_cursorPos - points.offset();
        prog.setUniformValue("cursorPos", relCursor.x, relCursor.y, relCursor.z);
        prog.setUniformValue("fileNumber", (GLint)(i + 1));
        prog.setUniformValue("pointPixelScale", (GLfloat)(0.5*width()*m_camera.projectionMatrix()(0,0)));
        totDrawn += points.draw(prog, globalCamPos, quality, simplify, incrementalDraw);
        glPopMatrix();
    }
    glDisable(GL_VERTEX_PROGRAM_POINT_SIZE);
    prog.release();
    return totDrawn;
}


/// Snap 3D cursor to closest point and centre the camera
void PointView::snapCursorAndCentre(double normalScaling)
{
    if(m_points.empty() && m_lines.empty() && m_meshes.empty())
        return;
    V3f N = (qt2exr(m_camera.position()) + m_drawOffset -
             m_cursorPos).normalized();
    V3d newPos(0);
    double nearestDist = DBL_MAX;
    if (!m_points.empty())
    {
        // Snap cursor to position of closest point and center on it
        for(size_t i = 0; i < m_points.size(); ++i)
        {
            if(m_points[i]->empty())
                continue;
            double dist = 0;
            size_t idx = m_points[i]->closestPoint(m_cursorPos, N,
                                                normalScaling, &dist);
            if(dist < nearestDist)
            {
                nearestDist = dist;
                newPos = m_points[i]->absoluteP(idx);
            }
        }
    }
    // FIXME: Remove this duplicate code!
    if (!m_meshes.empty())
    {
        for(size_t i = 0; i < m_meshes.size(); ++i)
        {
            double dist = 0;
            size_t idx = m_meshes[i]->closestVertex(m_cursorPos, N,
                                                    normalScaling, &dist);
            if(dist < nearestDist)
            {
                nearestDist = dist;
                newPos = m_meshes[i]->vertex(idx);
            }
        }
    }
    if (!m_lines.empty())
    {
        for(size_t i = 0; i < m_lines.size(); ++i)
        {
            double dist = 0;
            size_t idx = m_lines[i]->closestVertex(m_cursorPos, N,
                                                   normalScaling, &dist);
            if(dist < nearestDist)
            {
                nearestDist = dist;
                newPos = m_lines[i]->vertex(idx);
            }
        }
    }
    V3d posDiff = newPos - m_prevCursorSnap;
    tfm::printf("Selected %.3f [diff with previous = %.3f]\n",
                newPos, posDiff);
    m_cursorPos = newPos;
    m_prevCursorSnap = newPos;
    m_camera.setCenter(exr2qt(newPos - m_drawOffset));
    restartRender();
}


// vi: set et:
