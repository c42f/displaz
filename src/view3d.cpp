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
#include "view3d.h"

#include <QtCore/QTimer>
#include <QtCore/QTime>
#include <QtGui/QKeyEvent>
#include <QtGui/QLayout>
#include <QItemSelectionModel>
#include <QtOpenGL/QGLFramebufferObject>
#include <QMessageBox>
//#include <QtOpenGL/QGLBuffer>

#include "config.h"
#include "fileloader.h"
#include "qtlogger.h"
#include "mainwindow.h"
#include "mesh.h"
#include "shader.h"
#include "tinyformat.h"
#include "util.h"


//------------------------------------------------------------------------------
View3D::View3D(GeometryCollection* geometries, QWidget *parent)
    : QGLWidget(parent),
    m_camera(false, false),
    m_prevMousePos(0,0),
    m_mouseButton(Qt::NoButton),
    m_cursorPos(0),
    m_prevCursorSnap(0),
    m_backgroundColor(60, 50, 50),
    m_drawBoundingBoxes(true),
    m_drawCursor(true),
    m_badOpenGL(false),
    m_shaderProgram(),
    m_geometries(geometries),
    m_selectionModel(0),
    m_shaderParamsUI(0),
    m_incrementalFrameTimer(0),
    m_incrementalDraw(false)
{
    connect(m_geometries, SIGNAL(layoutChanged()),                      this, SLOT(geometryChanged()));
    //connect(m_geometries, SIGNAL(destroyed()),                          this, SLOT(modelDestroyed()));
    connect(m_geometries, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(geometryChanged()));
    connect(m_geometries, SIGNAL(rowsInserted(QModelIndex,int,int)),    this, SLOT(geometryInserted(const QModelIndex&, int,int)));
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


View3D::~View3D() { }


void View3D::restartRender()
{
    m_incrementalDraw = false;
    update();
}


void View3D::geometryChanged()
{
    if (m_geometries->rowCount() == 1)
        centerOnGeometry(m_geometries->index(0));
    restartRender();
}


void View3D::geometryInserted(const QModelIndex& /*unused*/, int firstRow, int lastRow)
{
    const GeometryCollection::GeometryVec& geoms = m_geometries->get();
    for (int i = firstRow; i <= lastRow; ++i)
        geoms[i]->initializeGL();
    geometryChanged();
}


void View3D::setShaderParamsUIWidget(QWidget* widget)
{
    m_shaderParamsUI = widget;
}


void View3D::setupShaderParamUI()
{
    if (!m_shaderProgram || !m_shaderParamsUI)
        return;
    while (QWidget* child = m_shaderParamsUI->findChild<QWidget*>())
        delete child;
    delete m_shaderParamsUI->layout();
    m_shaderProgram->setupParameterUI(m_shaderParamsUI);
}


void View3D::setSelectionModel(QItemSelectionModel* selectionModel)
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


void View3D::setBackground(QColor col)
{
    m_backgroundColor = col;
    restartRender();
}


void View3D::toggleDrawBoundingBoxes()
{
    m_drawBoundingBoxes = !m_drawBoundingBoxes;
    restartRender();
}

void View3D::toggleDrawCursor()
{
    m_drawCursor = !m_drawCursor;
    restartRender();
}

void View3D::toggleCameraMode()
{
    m_camera.setTrackballInteraction(!m_camera.trackballInteraction());
}


void View3D::centerOnGeometry(const QModelIndex& index)
{
    const Geometry& geom = *m_geometries->get()[index.row()];
    m_cursorPos = geom.centroid();
    m_camera.setCenter(m_cursorPos);
    double diag = (geom.boundingBox().max - geom.boundingBox().min).length();
    m_camera.setEyeToCenterDistance(std::max<double>(2*m_camera.clipNear(), diag*0.7));
}


void View3D::initializeGL()
{
    if (glewInit() != GLEW_OK)
    {
        g_logger.error("%s", "Failed to initialize GLEW");
        m_badOpenGL = true;
        return;
    }
    g_logger.info("OpenGL implementation:\n"
                  "GL_VENDOR   = %s\n"
                  "GL_RENDERER = %s\n"
                  "GL_VERSION  = %s",
                  (const char*)glGetString(GL_VENDOR),
                  (const char*)glGetString(GL_RENDERER),
                  (const char*)glGetString(GL_VERSION));
    m_shaderProgram->setContext(context());
    m_meshFaceShader.reset(new ShaderProgram(context()));
    m_meshFaceShader->setShaderFromSourceFile("shaders:meshface.glsl");
    m_meshEdgeShader.reset(new ShaderProgram(context()));
    m_meshEdgeShader->setShaderFromSourceFile("shaders:meshedge.glsl");
    m_incrementalFramebuffer = allocIncrementalFramebuffer(width(), height());
    const GeometryCollection::GeometryVec& geoms = m_geometries->get();
    for (size_t i = 0; i < geoms.size(); ++i)
        geoms[i]->initializeGL();
}


void View3D::resizeGL(int w, int h)
{
    if (m_badOpenGL)
        return;
    // Draw on full window
    glViewport(0, 0, w, h);
    m_camera.setViewport(QRect(0,0,w,h));
    m_incrementalFramebuffer = allocIncrementalFramebuffer(w,h);
}


std::unique_ptr<QGLFramebufferObject> View3D::allocIncrementalFramebuffer(int w, int h) const
{
    // TODO:
    // * Should we use multisampling 1 to avoid binding to a texture?
    const QGLFormat fmt = context()->format();
    QGLFramebufferObjectFormat fboFmt;
    fboFmt.setAttachment(QGLFramebufferObject::Depth);
    // Intel HD 3000 driver doesn't like the multisampling mode that Qt 4.8 uses
    // for samples==1, so work around it by forcing 0, if possible
    fboFmt.setSamples(fmt.samples() > 1 ? fmt.samples() : 0);
    //fboFmt.setTextureTarget();
    return std::unique_ptr<QGLFramebufferObject>(
        new QGLFramebufferObject(w, h, fboFmt));
}


void View3D::paintGL()
{
    if (m_badOpenGL)
        return;
    QTime frameTimer;
    frameTimer.start();

    m_incrementalFramebuffer->bind();

    //--------------------------------------------------
    // Draw main scene
    TransformState transState(Imath::V2i(width(), height()),
                              m_camera.projectionMatrix(),
                              m_camera.viewMatrix());

    glClearDepth(1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearColor(m_backgroundColor.redF(), m_backgroundColor.greenF(),
                 m_backgroundColor.blueF(), 1.0f);
    if (!m_incrementalDraw)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    std::vector<const Geometry*> geoms = selectedGeometry();

    // Draw bounding boxes
    if(m_drawBoundingBoxes && !m_incrementalDraw)
    {
        for(size_t i = 0; i < geoms.size(); ++i)
            drawBoundingBox(transState, geoms[i]->boundingBox(), Imath::C3f(1));
    }

    // Draw meshes and lines
    if (!m_incrementalDraw)
    {
        drawMeshes(transState, geoms);

        // Generic draw for any other geometry
        // (TODO: make all geometries use this interface, or something similar)
        // FIXME - Do generic quality scaling
        const double quality = 1;
        for (size_t i = 0; i < geoms.size(); ++i)
            geoms[i]->draw(transState, quality);
    }

    // Aim for 40ms frame time - an ok tradeoff for desktop usage
    const double targetMillisecs = 40;
    double quality = m_drawCostModel.quality(targetMillisecs, geoms, transState,
                                             m_incrementalDraw);

    // Render points
    DrawCount drawCount = drawPoints(transState, geoms, quality, m_incrementalDraw);

    // Measure frame time to update estimate for how much geometry we can draw
    // with a reasonable frame rate
    glFinish();
    int frameTime = frameTimer.elapsed();

    if (!geoms.empty())
        m_drawCostModel.addSample(drawCount, frameTime);

    // Debug: print bar showing how well we're sticking to the frame time
//    int barSize = 40;
//    std::string s = std::string(barSize*frameTime/targetMillisecs, '=');
//    if ((int)s.size() > barSize)
//        s[barSize] = '|';
//    tfm::printfln("%12f %4d %s", quality, frameTime, s);

    m_incrementalFramebuffer->release();
    QGLFramebufferObject::blitFramebuffer(0, QRect(0,0,width(),height()),
                                          m_incrementalFramebuffer.get(),
                                          QRect(0,0,width(),height()),
                                          GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Draw overlay stuff, including cursor position.
    if (m_drawCursor)
    {
        drawCursor(transState, m_cursorPos, 10, 1);
        //drawCursor(transState, m_camera.center(), 10);
    }

    // Set up timer to draw a high quality frame if necessary
    if (!drawCount.moreToDraw)
        m_incrementalFrameTimer->stop();
    else
        m_incrementalFrameTimer->start(10);
    m_incrementalDraw = true;
}


void View3D::drawMeshes(const TransformState& transState,
                        const std::vector<const Geometry*>& geoms) const
{
    // Draw faces
    if (m_meshFaceShader->isValid())
    {
        QGLShaderProgram& meshFaceShader = m_meshFaceShader->shaderProgram();
        meshFaceShader.bind();
        M44d worldToEyeVecTransform = m_camera.viewMatrix();
        worldToEyeVecTransform[3][0] = 0;
        worldToEyeVecTransform[3][1] = 0;
        worldToEyeVecTransform[3][2] = 0;
        V3d lightDir = V3d(1,1,-1).normalized() * worldToEyeVecTransform;
        meshFaceShader.setUniformValue("lightDir_eye", lightDir.x, lightDir.y, lightDir.z);
        for (size_t i = 0; i < geoms.size(); ++i)
            geoms[i]->drawFaces(meshFaceShader, transState);
        meshFaceShader.release();
    }

    // Draw edges
    if (m_meshEdgeShader->isValid())
    {
        QGLShaderProgram& meshEdgeShader = m_meshEdgeShader->shaderProgram();
        glLineWidth(1);
        meshEdgeShader.bind();
        for(size_t i = 0; i < geoms.size(); ++i)
            geoms[i]->drawEdges(meshEdgeShader, transState);
        meshEdgeShader.release();
    }
}


void View3D::mousePressEvent(QMouseEvent* event)
{
    m_mouseButton = event->button();
    m_prevMousePos = event->pos();
}


void View3D::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MidButton)
    {
        double snapScale = 0.025;
        QString pointInfo;
        V3d newPos = snapToGeometry(guessClickPosition(event->pos()), snapScale,
                                    pointInfo);
        V3d posDiff = newPos - m_prevCursorSnap;
        g_logger.info("Selected Point Attributes:\n"
                      "%s"
                      "diff with previous = %.3f\n"
                      "vector diff = %.3f",
                      pointInfo, posDiff.length(), posDiff);
        // Snap cursor /and/ camera to new position
        // TODO: Decouple these, but in a sensible way
        m_cursorPos = newPos;
        m_camera.setCenter(newPos);
        m_prevCursorSnap = newPos;
    }
}


void View3D::mouseMoveEvent(QMouseEvent* event)
{
    if (m_mouseButton == Qt::MidButton)
        return;
    bool zooming = m_mouseButton == Qt::RightButton;
    if(event->modifiers() & Qt::ControlModifier)
    {
        m_cursorPos = m_camera.mouseMovePoint(m_cursorPos,
                                              event->pos() - m_prevMousePos,
                                              zooming);
        restartRender();
    }
    else
    {
        m_camera.mouseDrag(m_prevMousePos, event->pos(), zooming);
    }
    m_prevMousePos = event->pos();
}


void View3D::wheelEvent(QWheelEvent* event)
{
    // Translate mouse wheel events into vertical dragging for simplicity.
    m_camera.mouseDrag(QPoint(0,0), QPoint(0, -event->delta()/2), true);
}


void View3D::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_C)
    {
        // Centre camera on current cursor location
        m_camera.setCenter(m_cursorPos);
    }
    else
        event->ignore();
}


/// Draw the 3D cursor
void View3D::drawCursor(const TransformState& transStateIn, const V3d& cursorPos,
                        float cursorRadius, float centerPointRadius) const
{
    V3d offset = transStateIn.cameraPos();
    TransformState transState = transStateIn.translate(offset);
    V3d relCursor = cursorPos - offset;

    // Cull if behind camera
    if((relCursor * transState.modelViewMatrix).z > 0)
        return;

    if (centerPointRadius > 0)
    {
        transState.load();
        // Draw a point at the center of the cursor.
        glColor3f(1,1,1);
        glPointSize(centerPointRadius);
        glBegin(GL_POINTS);
            glVertex(relCursor);
        glEnd();
    }

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

    // Find position of cursor in screen space
    V3d screenP3 = relCursor * transState.modelViewMatrix * transState.projMatrix;
    // Position in ortho coord system
    V2f p2 = 0.5f * V2f(width(), height()) *
             (V2f(screenP3.x, screenP3.y) + V2f(1.0f));
    float r1 = cursorRadius;
    float r2 = r1 + cursorRadius;
    glLineWidth(2);
    glColor3f(1,1,1);
    // Combined white and black crosshairs, so they can be seen on any
    // background.
    glTranslatef(p2.x, p2.y, 0);
    glBegin(GL_LINES);
        glVertex( V2f(r1, 0));
        glVertex( V2f(r2, 0));
        glVertex(-V2f(r1, 0));
        glVertex(-V2f(r2, 0));
        glVertex( V2f(0,  r1));
        glVertex( V2f(0,  r2));
        glVertex(-V2f(0,  r1));
        glVertex(-V2f(0,  r2));
    glEnd();
    glColor3f(0,0,0);
    glRotatef(45,0,0,1);
    glBegin(GL_LINES);
        glVertex( V2f(r1, 0));
        glVertex( V2f(r2, 0));
        glVertex(-V2f(r1, 0));
        glVertex(-V2f(r2, 0));
        glVertex( V2f(0,  r1));
        glVertex( V2f(0,  r2));
        glVertex(-V2f(0,  r1));
        glVertex(-V2f(0,  r2));
    glEnd();

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
}


/// Draw point cloud
DrawCount View3D::drawPoints(const TransformState& transState,
                             const std::vector<const Geometry*>& geoms,
                             double quality, bool incrementalDraw)
{
    DrawCount totDrawCount;
    if (geoms.empty())
        return DrawCount();
    if (!m_shaderProgram->isValid())
        return DrawCount();
    glEnable(GL_POINT_SPRITE);
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    // Draw points
    QGLShaderProgram& prog = m_shaderProgram->shaderProgram();
    prog.bind();
    m_shaderProgram->setUniforms();
    QModelIndexList selection = m_selectionModel->selectedRows();
    for(size_t i = 0; i < geoms.size(); ++i)
    {
        const Geometry& geom = *geoms[i];
        if(geom.pointCount() == 0)
            continue;
        V3f relCursor = m_cursorPos - geom.offset();
        prog.setUniformValue("cursorPos", relCursor.x, relCursor.y, relCursor.z);
        prog.setUniformValue("fileNumber", (GLint)(selection[(int)i].row() + 1));
        prog.setUniformValue("pointPixelScale", (GLfloat)(0.5*width()*m_camera.projectionMatrix()[0][0]));
        totDrawCount += geom.drawPoints(prog, transState, quality, incrementalDraw);
    }
    glDisable(GL_VERTEX_PROGRAM_POINT_SIZE);
    prog.release();
    return totDrawCount;
}


/// Guess position in 3D corresponding to a 2D click
///
/// `clickPos` - 2D position in viewport, as from a mouse event.
Imath::V3d View3D::guessClickPosition(const QPoint& clickPos)
{
    // Get new point in the projected coordinate system using the click
    // position x,y and the z of the reference position.  Take the reference to
    // be the camera rotation center since that's likely to be roughly the
    // depth the user is interested in.
    V3d refPos = m_camera.center();
    M44d mat = m_camera.viewMatrix()*m_camera.projectionMatrix()*m_camera.viewportMatrix();
    double refZ = (refPos * mat).z;
    V3d newPointProj(clickPos.x(), clickPos.y(), refZ);
    // Map projected point back into model coordinates
    return newPointProj * mat.inverse();
}


/// Snap `pos` to the perceptually closest piece of visible geometry
///
/// `normalScaling` - Distance along the camera direction will be scaled by
///                   this factor when computing the closest point.
Imath::V3d View3D::snapToGeometry(const Imath::V3d& pos, double normalScaling,
                                  QString& pointInfo)
{
    if (m_geometries->get().empty())
        return pos;
    // Ray out from the camera to the given point
    V3d cameraPos = m_camera.position();
    V3d viewDir = (pos - cameraPos).normalized();
    V3d newPos(0);
    double nearestDist = DBL_MAX;
    // Snap cursor to position of closest point and center on it
    QModelIndexList sel = m_selectionModel->selectedRows();
    for (int i = 0; i < sel.size(); ++i)
    {
        int geomIdx = sel[i].row();
        double dist = 0;
        std::string info;
        V3d p = m_geometries->get()[geomIdx]->pickVertex(cameraPos, pos, viewDir,
                                                         normalScaling, &dist, &info);
        if (dist < nearestDist)
        {
            newPos = p;
            nearestDist = dist;
            pointInfo = QString::fromStdString(info);
        }
    }
    return newPos;
}


/// Return list of currently selected geometry
std::vector<const Geometry*> View3D::selectedGeometry() const
{
    const GeometryCollection::GeometryVec& geomAll = m_geometries->get();
    QModelIndexList sel = m_selectionModel->selectedRows();
    std::vector<const Geometry*> geoms;
    geoms.reserve(sel.size());
    for(int i = 0; i < sel.size(); ++i)
        geoms.push_back(geomAll[sel[i].row()].get());
    return geoms;
}


// vi: set et:
