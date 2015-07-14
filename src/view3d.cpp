// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

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
    m_drawAxes(true),
    m_badOpenGL(false),
    m_shaderProgram(),
    m_geometries(geometries),
    m_selectionModel(0),
    m_shaderParamsUI(0),
    m_drawState(0),
    m_drawAxesBackground(QImage(":/resource/axes.png")),
    m_drawAxesLabelX(QImage(":/resource/x.png")),
    m_drawAxesLabelY(QImage(":/resource/y.png")),
    m_drawAxesLabelZ(QImage(":/resource/z.png"))
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

    m_drawTimer = new QTimer(this);
    m_drawTimer->setSingleShot(false);
    connect(m_drawTimer, SIGNAL(timeout()), this, SLOT(updateGL()));
    m_drawTimer->start(20);
}


View3D::~View3D() { }


void View3D::restartRender()
{
    m_drawState = DS_FIRST_DRAW;
    update();
}


void View3D::geometryChanged()
{
    if (m_geometries->rowCount() == 1)
        centerOnGeometry(m_geometries->index(0));
    else
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


void View3D::toggleDrawAxes()
{
    m_drawAxes = !m_drawAxes;
    restartRender();
}


void View3D::centerOnGeometry(const QModelIndex& index)
{
    const Geometry& geom = *m_geometries->get()[index.row()];
    m_cursorPos = geom.centroid();
    double diag = (geom.boundingBox().max - geom.boundingBox().min).length();
    m_camera.animateTo(m_camera.rotation(), m_cursorPos, std::max<double>(2*m_camera.clipNear(), diag*0.7));
}


void View3D::initializeGL()
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
    fboFmt.setSamples(fmt.samples());
    //fboFmt.setTextureTarget();
    return std::unique_ptr<QGLFramebufferObject>(
        new QGLFramebufferObject(w, h, fboFmt));
}


void View3D::paintGL()
{
    if(m_drawState == DS_NOTHING_TO_DRAW)
        return;

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

    std::vector<const Geometry*> geoms = selectedGeometry();

    if(m_drawState == DS_FIRST_DRAW)
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Draw bounding boxes
        if(m_drawBoundingBoxes)
        {
            for(size_t i = 0; i < geoms.size(); ++i)
               drawBoundingBox(transState, geoms[i]->boundingBox(), Imath::C3f(1));
        }

        // Draw meshes and lines
        drawMeshes(transState, geoms);

        // Generic draw for any other geometry
        // (TODO: make all geometries use this interface, or something similar)
        // FIXME - Do generic quality scaling
        const double quality = 1;
        for (size_t i = 0; i < geoms.size(); ++i)
            geoms[i]->draw(transState, quality);
    }

    // Aim for 40ms frame time - an ok tradeoff for desktop usage
    const double targetMillisecs = 20;
    double quality = m_drawCostModel.quality(targetMillisecs, geoms, transState,
                                             m_drawState == DS_ADDITIONAL_DRAW);

    // Render points
    DrawCount drawCount = drawPoints(transState, geoms, quality, m_drawState == DS_ADDITIONAL_DRAW);

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

    // Draw overlay axes
    if (m_drawAxes)
    {
        drawAxes();
    }

    // Set up timer to draw a high quality frame if necessary
    if (!drawCount.moreToDraw)
        m_drawState = DS_NOTHING_TO_DRAW;
    else if(m_mouseButton == Qt::NoButton && !m_camera.isAnimating())
        m_drawState = DS_ADDITIONAL_DRAW;
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

    if (event->button() == Qt::MidButton ||
        (event->button() == Qt::LeftButton && (event->modifiers() & Qt::ShiftModifier)))
    {
        double snapScale = 0.025;
        QString pointInfo;
        V3d newPos;
        if (snapToGeometry(guessClickPosition(event->pos()), snapScale,
                           &newPos, &pointInfo))
        {
            V3d posDiff = newPos - m_prevCursorSnap;
            g_logger.info("Selected Point Attributes:\n"
                          "%s"
                          "diff with previous = %.3f\n"
                          "vector diff = %.3f",
                          pointInfo, posDiff.length(), posDiff);
            // Snap cursor /and/ camera to new position
            // TODO: Decouple these, but in a sensible way

            // Reset view to from top
            m_cursorPos = newPos;
            m_camera.animateTo(m_camera.rotation(), newPos, m_camera.eyeToCenterDistance());
        }
    }
}


void View3D::mouseReleaseEvent(QMouseEvent* event)
{
    m_mouseButton = Qt::NoButton;
    paintGL();
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
    if(m_camera.isAnimating() || m_mouseButton != Qt::NoButton)
    {
        event->ignore();
        return;
    }

    if(event->key() == Qt::Key_C)
    {
        // Centre camera on current cursor location
        m_camera.animateTo(m_camera.rotation(), m_cursorPos, m_camera.eyeToCenterDistance());
    }
    else if(event->key() == Qt::Key_Z)
    {
        // Reset center and zoom to encompass selected geometries
        std::vector<const Geometry*> geoms = selectedGeometry();
        if(geoms.size() > 0)
        {
            Imath::Box3d bbox = geoms[0]->boundingBox();
            for (unsigned int i = 1; i < geoms.size(); ++i)
                bbox.extendBy(geoms[i]->boundingBox());
            m_cursorPos       = bbox.center();
            const double diag = (bbox.max - bbox.min).length();
            m_camera.animateTo(m_camera.rotation(), m_cursorPos, std::max<double>(2*m_camera.clipNear(), diag*0.7));
        }
    }
    else if(event->key() == Qt::Key_R)
    {
        // Snap to nearest axis position
        QQuaternion rot = m_camera.rotation();
        // snap view pos to nearest axis
        InteractiveCamera::snapRotationToAxis(QVector3D(0, 0, 1), rot);
        // snap view up to nearest axis
        InteractiveCamera::snapRotationToAxis(QVector3D(0, 1, 0), rot);
        m_camera.animateTo(rot, m_cursorPos, m_camera.eyeToCenterDistance());
    }
    else if(event->key() == Qt::Key_T)
    {
        // Rotate to top view
        m_camera.animateTo(QQuaternion(), m_cursorPos, m_camera.eyeToCenterDistance());
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

void View3D::drawAxes() const
{
    glPushAttrib(GL_ENABLE_BIT);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, width(), 0, height(), 0, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const GLint w = 64;    // Width of axes widget
    const GLint o = 8;     // Axes widget offset in x and y
    float transparency = 0.5;

    // Center of axis overlay
    const V3d center(o+w/2,o+w/2,0.0);

    // Background texture

    m_drawAxesBackground.bind();
    glColor4f(1,1,1,transparency);
    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
        glTexCoord2i(0,0); glVertex2i(o,   o  );
        glTexCoord2i(1,0); glVertex2i(o+w, o  );
        glTexCoord2i(1,1); glVertex2i(o+w, o+w);
        glTexCoord2i(0,1); glVertex2i(o,   o+w);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    // Compute perspective correct x,y,z axis directions at position of the
    // axis widget centre.  The full 3->window coordinate transformation is
    //
    //   c = a*M     // world -> clip coords
    //   n = c/c[3]  // Perspective divide (clip->NDC)
    //   e = n*W     // NDC -> window
    //
    // Taking the derivative de/da and rearranging gives the derivative:
    //
    //   de/da = (1/c[3]) * (M*W - outer_product(center, M[:,3]))
    //
    // The x-axis vector in the window coords is then [1,0,0,0] * de/da, etc.
    //
    // Using the projected axis directions can look a little weird, but so does
    // an orthographic projection.  The projected version has the advantage
    // that a line in the scene which is parallel to one of the x,y or z axes
    // and passes through the location of the axis widget is parallel to the
    // associated axis as drawn in the widget itself.
    const M44d M = m_camera.viewMatrix()*m_camera.projectionMatrix();

    // NDC->Window transform for the y=up,x=right window coordinate convention
    // used in the glOrtho() call above.  Note this is opposite of Qt's window
    // coordinates, which has y=0 at the top.
    //
    // Use zScreenScale so the size of the component into the screen is
    // comparable with x and y in the other directions.
    //
    // TODO: Fix the coordinate systems used so that they're consistently
    // manipulated with a common set of utilities.
    double zScreenScale = 0.5*std::max(width(),height());
    const M44d W = m_camera.viewportMatrix() *
                   Imath::M44d().setScale(V3d(1,-1,zScreenScale)) *
                   Imath::M44d().setTranslation(V3d(0,height(),0));

    const M44d A = M*W;
    V3d x = V3d(A[0][0],A[0][1],A[0][2]) - center*M[0][3];
    V3d y = V3d(A[1][0],A[1][1],A[1][2]) - center*M[1][3];
    V3d z = V3d(A[2][0],A[2][1],A[2][2]) - center*M[2][3];

    // Normalize axes to make them a predictable size in 2D.
    //
    // TODO: For a perspective transform this is actually a bit subtle, since
    // the magnitude of the component into the screen is affected by the clip
    // plane positions.
    x.normalize();
    y.normalize();
    z.normalize();

    // Ignore z component for drawing overlay
    x.z = y.z = z.z = 0.0;

    // Draw lines for the x, y and z directions
    {
        // color tint
        const double c = 0.8;
        const double d = 0.5;
        const double t = 0.5*(1+transparency);
        const double r = 0.6;  // 60% towards edge of circle
        glLineWidth(1);
        glBegin(GL_LINES);
            glColor4f(c,d,d,t);
            glVertex(center);
            glVertex(center + x*r*w/2);
            glColor4f(d,c,d,t);
            glVertex(center);
            glVertex(center + y*r*w/2);
            glColor4f(d,d,c,t);
            glVertex(center);
            glVertex(center + z*r*w/2);
        glEnd();
    }

    // Labels

    const double r = 0.8;   // 80% towards edge of circle
    const GLint l = 16;     // Label is 16 pixels wide

    glColor4f(1,1,1,transparency);
    glEnable(GL_TEXTURE_2D);

    // Note that V3d -> V3i (double to integer precision)
    // conversion is intentionally snapping the label to
    // integer co-ordinates to eliminate subpixel aliasing
    // artifacts.  This is also the reason that matrix
    // transformations are not being used for this.

    const V3d px = center+x*r*w/2;
    m_drawAxesLabelX.bind();
    glBegin(GL_QUADS);
        glTexCoord2i(0,0); glVertex(V3i(px+V3d(-l/2, l/2,0)));
        glTexCoord2i(0,1); glVertex(V3i(px+V3d(-l/2,-l/2,0)));
        glTexCoord2i(1,1); glVertex(V3i(px+V3d( l/2,-l/2,0)));
        glTexCoord2i(1,0); glVertex(V3i(px+V3d( l/2, l/2,0)));
    glEnd();

    const V3d py = center+y*r*w/2;
    m_drawAxesLabelY.bind();
    glBegin(GL_QUADS);
        glTexCoord2i(0,0); glVertex(V3i(py+V3d(-l/2, l/2,0)));
        glTexCoord2i(0,1); glVertex(V3i(py+V3d(-l/2,-l/2,0)));
        glTexCoord2i(1,1); glVertex(V3i(py+V3d( l/2,-l/2,0)));
        glTexCoord2i(1,0); glVertex(V3i(py+V3d( l/2, l/2,0)));
    glEnd();

    const V3d pz = center+z*r*w/2;
    m_drawAxesLabelZ.bind();
    glBegin(GL_QUADS);
        glTexCoord2i(0,0); glVertex(V3i(pz+V3d(-l/2, l/2,0)));
        glTexCoord2i(0,1); glVertex(V3i(pz+V3d(-l/2,-l/2,0)));
        glTexCoord2i(1,1); glVertex(V3i(pz+V3d( l/2,-l/2,0)));
        glTexCoord2i(1,0); glVertex(V3i(pz+V3d( l/2, l/2,0)));
    glEnd();

    glDisable(GL_TEXTURE_2D);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();

    glBindTexture(GL_TEXTURE_2D, 0);
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
    // position x,y and the z of a reference position.  Take the reference point
    // of interest to be between the camera rotation center and the camera
    // position, as a rough guess of the depth the user is interested in.
    //
    // This works pretty well, except when there are noise points intervening
    // between the reference position and the user's actual point of interest.
    V3d refPos = 0.7*m_camera.position() + 0.3*m_camera.center();
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
/// `newPos`        - Returned new position
/// `pointInfo`     - Returned descriptive string of point attributes.
///                   Ignored if null.
///
/// Returns true if a close piece of geometry was found, false otherwise.
bool View3D::snapToGeometry(const Imath::V3d& pos, double normalScaling,
                            Imath::V3d* newPos, QString* pointInfo)
{
    // Ray out from the camera to the given point
    V3d cameraPos = m_camera.position();
    V3d viewDir = (pos - cameraPos).normalized();
    double nearestDist = DBL_MAX;
    // Snap cursor to position of closest point and center on it
    QModelIndexList sel = m_selectionModel->selectedRows();
    for (int i = 0; i < sel.size(); ++i)
    {
        int geomIdx = sel[i].row();
        V3d pickedVertex;
        double dist = 0;
        std::string info;
        if(m_geometries->get()[geomIdx]->pickVertex(cameraPos, pos, viewDir, normalScaling,
                                                    pickedVertex, &dist,
                                                    (pointInfo != 0) ? &info : 0))
        {
            if (dist < nearestDist)
            {
                *newPos = pickedVertex;
                nearestDist = dist;
                if (pointInfo)
                    *pointInfo = QString::fromStdString(info);
            }
        }
    }
    return nearestDist < DBL_MAX;
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
