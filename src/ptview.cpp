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

#include "glutil.h"
#include "ptview.h"
#include "mainwindow.h"
#include "shader.h"

#include "config.h"
#include "logger.h"
#include "util.h"

#include <QtCore/QTimer>
#include <QtCore/QTime>
#include <QtGui/QKeyEvent>
#include <QtGui/QLayout>
#include <QItemSelectionModel>
#include <QtOpenGL/QGLFramebufferObject>
#include <QMessageBox>
//#include <QtOpenGL/QGLBuffer>

#include "fileloader.h"
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
PointView::PointView(GeometryCollection* geometries, QWidget *parent)
    : QGLWidget(parent),
    m_camera(false, false),
    m_prevMousePos(0,0),
    m_mouseButton(Qt::NoButton),
    m_cursorPos(0),
    m_prevCursorSnap(0),
    m_drawOffset(0),
    m_backgroundColor(60, 50, 50),
    m_drawBoundingBoxes(true),
    m_drawCursor(true),
    m_badOpenGL(false),
    m_shaderProgram(),
    m_geometries(geometries),
    m_selectionModel(0),
    m_shaderParamsUI(0),
    m_incrementalFrameTimer(0),
    m_incrementalDraw(false),
    m_maxPointsPerFrame(g_defaultPointRenderCount)
{
    connect(m_geometries, SIGNAL(layoutChanged()),                      this, SLOT(geometryChanged()));
    //connect(m_geometries, SIGNAL(destroyed()),                          this, SLOT(modelDestroyed()));
    connect(m_geometries, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(geometryChanged()));
    connect(m_geometries, SIGNAL(rowsInserted(QModelIndex,int,int)),    this, SLOT(geometryChanged()));
    connect(m_geometries, SIGNAL(rowsRemoved(QModelIndex,int,int)),     this, SLOT(geometryChanged()));

    setSelectionModel(new QItemSelectionModel(m_geometries, this));

    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    m_camera.setClipFar(FLT_MAX);
    // Setting a good value for the near camera clipping plane is difficult
    // when trying to show a large variation of length scales:  Setting a very
    // small value allows us to see objects very close to the camera; the
    // tradeoff is that this reduces the resolution of the z-buffer leading to
    // z-fighting in the distance.
    m_camera.setClipNear(1);
    connect(&m_camera, SIGNAL(projectionChanged()), this, SLOT(restartRender()));
    connect(&m_camera, SIGNAL(viewChanged()), this, SLOT(restartRender()));

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
    update();
}


void PointView::geometryChanged()
{
    if (m_geometries->rowCount() == 1)
        centreOnGeometry(m_geometries->index(0));
    //m_maxPointsPerFrame = g_defaultPointRenderCount;
    setupShaderParamUI(); // Ugh, file name list changed.  FIXME: Kill this off
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
    for(size_t i = 0; i < m_geometries->get().size(); ++i)
    {
        if (m_geometries->get()[i]->pointCount() > 0)
            fileNames << QFileInfo(m_geometries->get()[i]->fileName()).fileName();
    }
    m_shaderProgram->setupParameterUI(m_shaderParamsUI, fileNames);
}


void PointView::setSelectionModel(QItemSelectionModel* selectionModel)
{
    assert(selectionModel);
    if (selectionModel->model() != m_geometries)
    {
        assert(0 && "Attempt to set incompatible selection model");
        return;
    }
    if (m_selectionModel)
    {
        disconnect(m_selectionModel, SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
                   this, SLOT(restartRender()));
    }
    m_selectionModel = selectionModel;
    connect(m_selectionModel, SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this, SLOT(restartRender()));
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

void PointView::toggleDrawCursor()
{
    m_drawCursor = !m_drawCursor;
    restartRender();
}

void PointView::toggleCameraMode()
{
    m_camera.setTrackballInteraction(!m_camera.trackballInteraction());
}


void PointView::centreOnGeometry(const QModelIndex& index)
{
    const Geometry& geom = *m_geometries->get()[index.row()];
    m_cursorPos = geom.centroid();
    m_drawOffset = geom.offset();
    m_camera.setCenter(exr2qt(m_cursorPos - m_drawOffset));
    double diag = (geom.boundingBox().max - geom.boundingBox().min).length();
    m_camera.setEyeToCenterDistance(std::max<double>(2*m_camera.clipNear(), diag*0.7));
}


void PointView::initializeGL()
{
    if (glewInit() != GLEW_OK)
    {
        g_logger.error("%s", "Failed to initialize GLEW");
        m_badOpenGL = true;
        return;
    }
    m_shaderProgram->setContext(context());
    m_meshFaceShader.reset(new ShaderProgram(context()));
    m_meshFaceShader->setShaderFromSourceFile("shaders:meshface.glsl");
    m_meshEdgeShader.reset(new ShaderProgram(context()));
    m_meshEdgeShader->setShaderFromSourceFile("shaders:meshedge.glsl");
    m_incrementalFramebuffer = allocIncrementalFramebuffer(width(), height());
}


void PointView::resizeGL(int w, int h)
{
    if (m_badOpenGL)
        return;
    // Draw on full window
    glViewport(0, 0, w, h);
    m_camera.setViewport(QRect(0,0,w,h));
    m_incrementalFramebuffer = allocIncrementalFramebuffer(w,h);
}


std::unique_ptr<QGLFramebufferObject> PointView::allocIncrementalFramebuffer(int w, int h) const
{
    // TODO:
    // * Should we use multisampling 1 to avoid binding to a texture?
    const QGLFormat fmt = context()->format();
    QGLFramebufferObjectFormat fboFmt;
    fboFmt.setAttachment(QGLFramebufferObject::Depth);
    fboFmt.setSamples(fmt.samples());
    //fboFmt.setTextureTarget();
    return std::unique_ptr<QGLFramebufferObject>(
        new QGLFramebufferObject(w, h, fboFmt));
}


void PointView::paintGL()
{
    if (m_badOpenGL)
        return;
    QTime frameTimer;
    frameTimer.start();

    m_incrementalFramebuffer->bind();

    //--------------------------------------------------
    // Draw main scene
    TransformState transState(qt2exr(m_camera.projectionMatrix()),
                              qt2exr(m_camera.viewMatrix()));

    glClearDepth(1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearColor(m_backgroundColor.redF(), m_backgroundColor.greenF(),
                 m_backgroundColor.blueF(), 1.0f);
    if (!m_incrementalDraw)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const GeometryCollection::GeometryVec& geoms = m_geometries->get();
    QModelIndexList sel = m_selectionModel->selectedRows();

    // Draw bounding boxes
    if(m_drawBoundingBoxes && !m_incrementalDraw)
    {
        for(int i = 0; i < sel.size(); ++i)
        {
            // Draw bounding box
            Imath::Box3d bbox = geoms[sel[i].row()]->boundingBox();
            bbox.min -= m_drawOffset;
            bbox.max -= m_drawOffset;
            drawBoundingBox(transState, bbox, Imath::C3f(1));
        }
    }

    // Draw meshes and lines
    if (!m_incrementalDraw)
    {
        drawMeshes(transState, geoms, sel);
    }

    // Figure out how many points we should render
    size_t totPoints = 0;
    for (int i = 0; i < sel.size(); ++i)
        totPoints += geoms[sel[i].row()]->pointCount();
    size_t numPointsToRender = std::min(totPoints, m_maxPointsPerFrame);
    size_t totDrawn = 0;
    // Render points
    totDrawn = drawPoints(transState, geoms, sel, numPointsToRender, m_incrementalDraw);

    // Measure frame time to update estimate for how many points we can
    // draw interactively
    glFinish();
    if (totPoints > 0 && !m_incrementalDraw)
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
        // Prevent any insanity from this getting too small
        m_maxPointsPerFrame = std::max(m_maxPointsPerFrame, (size_t)100);
    }
    m_incrementalFramebuffer->release();
    QGLFramebufferObject::blitFramebuffer(0, QRect(0,0,width(),height()),
                                          m_incrementalFramebuffer.get(),
                                          QRect(0,0,width(),height()),
                                          GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Draw overlay stuff, including cursor position.
    if (m_drawCursor)
        drawCursor(m_cursorPos - m_drawOffset);

    // Set up timer to draw a high quality frame if necessary
    if (totDrawn == 0)
        m_incrementalFrameTimer->stop();
    else
        m_incrementalFrameTimer->start(10);
    m_incrementalDraw = true;
}


void PointView::drawMeshes(const TransformState& transState,
                           const GeometryCollection::GeometryVec& geoms,
                           const QModelIndexList& sel) const
{
    // Draw faces
    QGLShaderProgram& meshFaceShader = m_meshFaceShader->shaderProgram();
    meshFaceShader.bind();
    meshFaceShader.setUniformValue("lightDir_eye",
                m_camera.viewMatrix().mapVector(QVector3D(1,1,-1).normalized()));
    for (int i = 0; i < sel.size(); ++i)
    {
        V3d offset = geoms[sel[i].row()]->offset() - m_drawOffset;
        transState.translate(offset).setUniforms(meshFaceShader.programId());
        geoms[sel[i].row()]->drawFaces(meshFaceShader);
    }
    meshFaceShader.release();

    // Draw edges
    QGLShaderProgram& meshEdgeShader = m_meshEdgeShader->shaderProgram();
    glLineWidth(1);
    meshEdgeShader.bind();
    for(int i = 0; i < sel.size(); ++i)
    {
        V3d offset = geoms[sel[i].row()]->offset() - m_drawOffset;
        transState.translate(offset).setUniforms(meshEdgeShader.programId());
        geoms[sel[i].row()]->drawEdges(meshEdgeShader);
    }
    meshEdgeShader.release();
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
    }
    m_prevMousePos = event->pos();
}


void PointView::wheelEvent(QWheelEvent* event)
{
    // Translate mouse wheel events into vertical dragging for simplicity.
    m_camera.mouseDrag(QPoint(0,0), QPoint(0, -event->delta()/2), true);
}


void PointView::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_C)
    {
        // Centre camera on current cursor location
        m_camera.setCenter(exr2qt(m_cursorPos - m_drawOffset));
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
size_t PointView::drawPoints(const TransformState& transState,
                             const GeometryCollection::GeometryVec& geoms,
                             const QModelIndexList& selection,
                             size_t numPointsToRender,
                             bool incrementalDraw)
{
    if (selection.empty())
        return 0;
    glEnable(GL_POINT_SPRITE);
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    V3d globalCamPos = qt2exr(m_camera.position()) + m_drawOffset;
    double quality = 1;
    // Get total number of points we would draw at quality == 1
    size_t totSimplified = 0;
    for(int i = 0; i < selection.size(); ++i)
        totSimplified += geoms[selection[i].row()]->simplifiedPointCount(globalCamPos,
                                                                         incrementalDraw);
    quality = double(numPointsToRender) / totSimplified;
    // Draw points
    QGLShaderProgram& prog = m_shaderProgram->shaderProgram();
    prog.bind();
    m_shaderProgram->setUniforms();
    size_t totDrawn = 0;
    for(int i = 0; i < selection.size(); ++i)
    {
        int geomIdx = selection[i].row();
        Geometry& geom = *geoms[geomIdx];
        if(geom.pointCount() == 0)
            continue;
        V3d offset = geom.offset() - m_drawOffset;
        //tfm::printf("offset = %.3f\n", offset);
        transState.translate(offset).setUniforms(prog.programId());
        V3f relCursor = m_cursorPos - geom.offset();
        prog.setUniformValue("cursorPos", relCursor.x, relCursor.y, relCursor.z);
        prog.setUniformValue("fileNumber", (GLint)(geomIdx + 1));
        prog.setUniformValue("pointPixelScale", (GLfloat)(0.5*width()*m_camera.projectionMatrix()(0,0)));
        totDrawn += geom.drawPoints(prog, globalCamPos, quality, incrementalDraw);
    }
    glDisable(GL_VERTEX_PROGRAM_POINT_SIZE);
    prog.release();
    return totDrawn;
}


/// Snap 3D cursor to closest point and centre the camera
void PointView::snapCursorAndCentre(double normalScaling)
{
    if(m_geometries->get().empty())
        return;
    V3f N = (qt2exr(m_camera.position()) + m_drawOffset -
             m_cursorPos).normalized();
    V3d newPos(0);
    double nearestDist = DBL_MAX;
    // Snap cursor to position of closest point and center on it
    for(size_t i = 0; i < m_geometries->get().size(); ++i)
    {
        double dist = 0;
        V3d p = m_geometries->get()[i]->pickVertex(m_cursorPos, N,
                                            normalScaling, &dist);
        if(dist < nearestDist)
        {
            newPos = p;
            nearestDist = dist;
        }
    }
    V3d posDiff = newPos - m_prevCursorSnap;
    g_logger.info("Selected %.3f\n"
                  "    [diff with previous: %.3f m;\n"
                  "     %.3f]",
                  newPos, posDiff.length(), posDiff);
    m_cursorPos = newPos;
    m_prevCursorSnap = newPos;
    m_camera.setCenter(exr2qt(newPos - m_drawOffset));
}


// vi: set et:
