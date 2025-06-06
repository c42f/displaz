// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

// DEBUG enable OpenGL error checking
// #define GL_CHECK 1

#include "glutil.h"
#include "gldebug.h"

#include "View3D.h"

#include <cfloat>

#include <QTimer>
#include <QAction>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLayout>
#include <QItemSelectionModel>
#include <QMessageBox>
#include <QGLFormat>
#include <QSettings>

#include "config.h"
#include "fileloader.h"
#include "QtLogger.h"
#include "MainWindow.h"
#include "DataSetUI.h"
#include "TriMesh.h"
#include "Enable.h"
#include "Shader.h"
#include "ShaderProgram.h"
#include "tinyformat.h"
#include "util.h"

//------------------------------------------------------------------------------
View3D::View3D(GeometryCollection* geometries, const QGLFormat& format, MainWindow *parent, DataSetUI *dataSet)
    : QGLWidget(format, parent),
    m_mainWindow(parent),
    m_dataSet(dataSet),
    m_mouseButton(Qt::NoButton),
    m_explicitCursorPos(false),
    m_cursorPos(0),
    m_backgroundColor(60, 50, 50),
    m_badOpenGL(false),
    m_geometries(geometries),
    m_shaderParamsUI(0),
    m_incrementalFrameTimer(0),
    m_incrementalDraw(false),
    m_devicePixelRatio(1.0)
{
    connect(m_geometries, SIGNAL(layoutChanged()),                      this, SLOT(geometryChanged()));
    //connect(m_geometries, SIGNAL(destroyed()),                          this, SLOT(modelDestroyed()));
    connect(m_geometries, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(geometryChanged()));
    connect(m_geometries, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(dataChanged(QModelIndex,QModelIndex)));
    connect(m_geometries, SIGNAL(rowsInserted(QModelIndex,int,int)),    this, SLOT(geometryInserted(const QModelIndex&, int,int)));
    connect(m_geometries, SIGNAL(rowsRemoved(QModelIndex,int,int)),     this, SLOT(geometryChanged()));

    setSelectionModel(new QItemSelectionModel(m_geometries, this));

    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    setFocus();

    connect(&m_camera, SIGNAL(projectionChanged()), this, SLOT(restartRender()));
    connect(&m_camera, SIGNAL(viewChanged()), this, SLOT(restartRender()));

    makeCurrent();
    m_shaderProgram = std::make_unique<ShaderProgram>();
    connect(m_shaderProgram.get(), SIGNAL(uniformValuesChanged()),
            this, SLOT(restartRender()));
    connect(m_shaderProgram.get(), SIGNAL(shaderChanged()),
            this, SLOT(restartRender()));
    connect(m_shaderProgram.get(), SIGNAL(paramsChanged()),
            this, SLOT(setupShaderParamUI()));
    m_enable = std::make_unique<Enable>();

    m_incrementalFrameTimer = new QTimer(this);
    m_incrementalFrameTimer->setSingleShot(false);
    connect(m_incrementalFrameTimer, SIGNAL(timeout()), this, SLOT(updateGL()));

    // Actions

    m_boundingBoxAction = new QAction(tr("Draw Bounding bo&xes"), this);
    m_boundingBoxAction->setCheckable(true);
    m_boundingBoxAction->setChecked(m_drawBoundingBoxes);
    connect(m_boundingBoxAction, SIGNAL(toggled(bool)), this, SLOT(setBoundingBox(bool)));

    m_cursorAction = new QAction(tr("Draw 3D &Cursor"), this);
    m_cursorAction->setCheckable(true);
    m_cursorAction->setChecked(m_drawCursor);
    connect(m_cursorAction, SIGNAL(toggled(bool)), this, SLOT(setCursor(bool)));

    m_axesAction = new QAction(tr("Draw &Axes"), this);
    m_axesAction->setCheckable(true);
    m_axesAction->setChecked(m_drawAxes);
    connect(m_axesAction, SIGNAL(toggled(bool)), this, SLOT(setAxes(bool)));

    m_gridAction = new QAction(tr("Draw &Grid"), this);
    m_gridAction->setCheckable(true);
    m_gridAction->setChecked(m_drawGrid);
    connect(m_gridAction, SIGNAL(toggled(bool)), this, SLOT(setGrid(bool)));

    m_annotationAction = new QAction(tr("Draw A&nnotations"), this);
    m_annotationAction->setCheckable(true);
    m_annotationAction->setChecked(m_drawAnnotations);
    connect(m_annotationAction, SIGNAL(toggled(bool)), this, SLOT(setAnnotations(bool)));
}

void View3D::restartRender()
{
    m_incrementalDraw = false;
    update();
}

void View3D::geometryChanged()
{
    restartRender();
}

/// Initialize all geometry in m_geometries for indices i in [begin,end)
void View3D::initializeGLGeometry(int begin, int end)
{
    const GeometryCollection::GeometryVec& geoms = m_geometries->get();
    for (int i = begin; i < end; ++i)
    {
        if (m_boundingBoxShader->isValid())
        {
            // TODO: build a shader manager for this
            geoms[i]->setShaderId("boundingbox", m_boundingBoxShader->shaderProgram().programId());
            geoms[i]->setShaderId("meshface", m_meshFaceShader->shaderProgram().programId());
            geoms[i]->setShaderId("meshedge", m_meshEdgeShader->shaderProgram().programId());
            geoms[i]->setShaderId("annotation", m_annotationShader->shaderProgram().programId());
            geoms[i]->initializeGL();
        }
    }
}

void View3D::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    initializeGLGeometry(topLeft.row(), bottomRight.row()+1);
    geometryChanged();
}

void View3D::geometryInserted(const QModelIndex& /*unused*/, int firstRow, int lastRow)
{
    // NB: Geometry inserted at indices i in [firstRow,lastRow]  (end inclusive)
    initializeGLGeometry(firstRow, lastRow+1);
    if (m_geometries->rowCount() == (lastRow+1-firstRow) && !m_explicitCursorPos)
    {
        // When loading first geometry (or geometries), centre on it.
        centerOnGeometry(m_geometries->index(0));
    }
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


void View3D::addAnnotation(const QString& label, const QString& text,
                           const Imath::V3d& pos)
{
    GLuint programId = m_annotationShader->shaderProgram().programId();
    auto annotation = std::make_shared<Annotation>(label, programId, text, pos);
    m_annotations.append(annotation);
    restartRender();
}


void View3D::removeAnnotations(const QRegExp& labelRegex)
{
    QVector<std::shared_ptr<Annotation>> annotations;
    foreach (auto annotation, m_annotations)
    {
        if (!labelRegex.exactMatch(annotation->label()))
            annotations.append(annotation);
    }
    m_annotations = annotations;
    restartRender();
}


void View3D::setBackground(QColor col)
{
    m_backgroundColor = col;
    restartRender();
}


void View3D::setBoundingBox(bool enable)
{
    m_drawBoundingBoxes = enable;
    restartRender();
}

void View3D::setCursor(bool enable)
{
    m_drawCursor = enable;
    restartRender();
}

void View3D::setAxes(bool enable)
{
    m_drawAxes = enable;
    restartRender();
}

void View3D::setGrid(bool enable)
{
    m_drawGrid = enable;
    restartRender();
}

void View3D::setAnnotations(bool enable)
{
    m_drawAnnotations = enable;
    restartRender();
}

void View3D::centerOnGeometry(const QModelIndex& index)
{
    const Geometry& geom = *m_geometries->get()[index.row()];
    m_cursorPos = geom.centroid();
    m_camera.setCenter(m_cursorPos);
    double diag = (geom.boundingBox().max - geom.boundingBox().min).length();
    m_camera.setEyeToCenterDistance(diag*0.7 + 0.01);
}

void View3D::centerOnPoint(const Imath::V3d& pos)
{
    m_cursorPos = pos;
    m_camera.setCenter(m_cursorPos);
}

void View3D::setExplicitCursorPos(const Imath::V3d& pos)
{
    m_explicitCursorPos = true;
    centerOnPoint(pos);
}


double View3D::getDevicePixelRatio()
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    return devicePixelRatioF();
#else
    return devicePixelRatio();
#endif
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
                  "  GL_VENDOR    = %s\n"
                  "  GL_RENDERER  = %s\n"
                  "  GL_VERSION   = %s\n"
                  "  GLSL_VERSION = %s\n"
                  "  GLEW_VERSION = %s",
                  (const char*)glGetString(GL_VENDOR),
                  (const char*)glGetString(GL_RENDERER),
                  (const char*)glGetString(GL_VERSION),
                  (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION),
                  (const char*)glewGetString(GLEW_VERSION));

    g_logger.info("OpenGL format: %s%s%s%s%s",
                  format().doubleBuffer() ? "double " : "",
                  format().stencil() ? "stencil " : "",
                  format().alpha() ? "alpha " : "",
                  format().accum() ? "accum " : "",
                  format().stereo() ? "stereo " : "");

    // GL_CHECK has to be defined for this to actually do something
    glCheckError();

    initCursor(10, 1);
    initAxes();
    initGrid(2.0f);

    glCheckError();

    m_boundingBoxShader.reset(new ShaderProgram());
    m_boundingBoxShader->setShaderFromSourceFile("shaders:bounding_box.glsl");

    m_meshFaceShader.reset(new ShaderProgram());
    m_meshFaceShader->setShaderFromSourceFile("shaders:meshface.glsl");

    m_meshEdgeShader.reset(new ShaderProgram());
    m_meshEdgeShader->setShaderFromSourceFile("shaders:meshedge.glsl");

    m_annotationShader.reset(new ShaderProgram());
    m_annotationShader->setShaderFromSourceFile("shaders:annotation.glsl");

    double dPR = getDevicePixelRatio();
    int w = width() * dPR;
    int h = height() * dPR;

    m_incrementalFramebuffer.init(w, h);

    initializeGLGeometry(0, m_geometries->get().size());

    // FIXME: Do something about this mess.  The shader editor widget needs to
    // be initialized with the default shader, but View3D can only compile
    // shaders after it has a valid OpenGL context.
    MainWindow * pv_parent = dynamic_cast<MainWindow *>(parentWidget());
    if (pv_parent)
        pv_parent->openShaderFile(QString());

    setFocus();

    glCheckError();
}


void View3D::resizeGL(int w, int h)
{
    //Note: This function receives "device pixel sizes" for correct OpenGL viewport setup
    //      Under OS X with retina display, this becomes important as it is 2x the width() or height()
    //      of the normal window (there is a devicePixelRatio() function to deal with this).

    if (m_badOpenGL)
        return;

    // Draw on full window
    glViewport(0, 0, w, h);

    double dPR = getDevicePixelRatio();

    m_camera.setViewport(QRect(0,0,double(w)/dPR,double(h)/dPR));

    m_incrementalFramebuffer.init(w,h);
    glCheckError();
}


void View3D::paintGL()
{
    if (m_badOpenGL)
    {
        return;
    }

    QElapsedTimer frameTimer;
    frameTimer.start();

    // Get window size
    double dPR = getDevicePixelRatio();
    int w = width() * dPR;
    int h = height() * dPR;

    // detecting a change in the device pixel ratio, since only the new QWindow (Qt5) would
    // provide a signal for screen changes and this is the easiest solution
    if (dPR != m_devicePixelRatio)
    {
        m_devicePixelRatio = dPR;
        resizeGL(w, h);
    }

    glCheckError();

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_incrementalFramebuffer.id());

    //--------------------------------------------------
    // Draw main scene
    TransformState transState(Imath::V2i(w, h),
                              m_camera.projectionMatrix(),
                              m_camera.viewMatrix());

    glClearDepth(1.0f);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthFunc(GL_LEQUAL);
    glClearColor(m_backgroundColor.redF(), m_backgroundColor.greenF(),
                 m_backgroundColor.blueF(), 1.0f);
    glClearStencil(0);
    if (!m_incrementalDraw)
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }

    std::vector<const Geometry*> geoms = selectedGeometry();

    // Aim for 40ms frame time - an ok tradeoff for desktop usage
    const double targetMillisecs = 40;
    double quality = m_drawCostModel.quality(targetMillisecs, geoms, transState,
                                             m_incrementalDraw);

    // Render points
    DrawCount drawCount = drawPoints(transState, geoms, quality, m_incrementalDraw);

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

    // Measure frame time to update estimate for how much geometry we can draw
    // with a reasonable frame rate
    glFinish();
    int frameTime = frameTimer.elapsed();

    glCheckError();

    if (!geoms.empty())
        m_drawCostModel.addSample(drawCount, frameTime);

    // Debug: print bar showing how well we're sticking to the frame time
//    int barSize = 40;
//    std::string s = std::string(barSize*frameTime/targetMillisecs, '=');
//    if ((int)s.size() > barSize)
//        s[barSize] = '|';
//    tfm::printfln("%12f %4d %s", quality, frameTime, s);

    // TODO: this should really render a texture onto a quad and not use glBlitFramebuffer
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_incrementalFramebuffer.id());
    glBlitFramebuffer(0,0,w,h, 0,0,w,h,
                      GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST); // has to be GL_NEAREST to work with DEPTH

    glCheckError();

    // Draw a grid for orientation purposes
    if (m_drawGrid)
    {
        drawGrid();
    }

    // Draw bounding boxes
    if (m_drawBoundingBoxes)
    {
        if (m_boundingBoxShader->isValid())
        {
            QOpenGLShaderProgram &boundingBoxShader = m_boundingBoxShader->shaderProgram();
            // shader
            boundingBoxShader.bind();
            // matrix stack
            transState.setUniforms(boundingBoxShader.programId());

            for (size_t i = 0; i < geoms.size(); ++i)
            {
                drawBoundingBox(transState, geoms[i]->getVAO("boundingbox"), geoms[i]->boundingBox().min,
                                Imath::C3f(1), geoms[i]->shaderId("boundingbox")); //boundingBoxShader.programId()
            }
        }
    }

    // Draw overlay stuff, including cursor position.
    if (m_drawCursor)
    {
        drawCursor(transState, m_cursorPos, 10);
        //drawCursor(transState, m_camera.center(), 10);
    }

    // Draw overlay axes
    if (m_drawAxes)
    {
        drawAxes();
    }

    // Draw annotations
    if (m_drawAnnotations && m_annotationShader->isValid())
    {
        drawAnnotations(transState, w, h);
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
        QOpenGLShaderProgram& meshFaceShader = m_meshFaceShader->shaderProgram();
        meshFaceShader.bind();
        M44d worldToEyeVecTransform = m_camera.viewMatrix();
        worldToEyeVecTransform[3][0] = 0;
        worldToEyeVecTransform[3][1] = 0;
        worldToEyeVecTransform[3][2] = 0;
        V3d lightDir = V3d(1,1,-1).normalized() * worldToEyeVecTransform;
        meshFaceShader.setUniformValue("lightDir_eye", lightDir.x, lightDir.y, lightDir.z);
        for (size_t i = 0; i < geoms.size(); ++i)
            geoms[i]->drawFaces(meshFaceShader, transState);
    }

    // Draw edges
    if (m_meshEdgeShader->isValid())
    {
        QOpenGLShaderProgram& meshEdgeShader = m_meshEdgeShader->shaderProgram();
        glLineWidth(1.0f);
        meshEdgeShader.bind();
        for (size_t i = 0; i < geoms.size(); ++i)
            geoms[i]->drawEdges(meshEdgeShader, transState);
    }
}

void View3D::drawAnnotations(const TransformState& transState,
                             int viewportPixelWidth,
                             int viewportPixelHeight) const
{
    QOpenGLShaderProgram& annotationShader = m_annotationShader->shaderProgram();
    annotationShader.bind();
    annotationShader.setUniformValue("viewportSize",
                                     viewportPixelWidth,
                                     viewportPixelHeight);
    // TODO: Draw further annotations first for correct ordering. Not super
    // important because annotations rarely overlap.
    for (auto annotation : m_annotations)
        annotation->draw(m_annotationShader->shaderProgram(), transState);
}

void View3D::mousePressEvent(QMouseEvent* event)
{
    m_mouseButton = event->button();
    m_prevMousePos = event->pos();

    if (event->button() == Qt::MiddleButton ||
        (event->button() == Qt::LeftButton && (event->modifiers() & Qt::ShiftModifier)))
    {
        snapToPoint(guessClickPosition(event->pos()));
    }
}

void View3D::snapToPoint(const Imath::V3d & pos)
{

    double snapScale = 0.025;
    QString pointInfo;
    V3d newPos(0.0,0.0,0.0); //init newPos to origin
    if (snapToGeometry(pos, snapScale,
                &newPos, &pointInfo))
    {
        if (m_prevCursorSnap)
        {
            V3d posDiff = newPos - *m_prevCursorSnap;
            g_logger.info("Selected Point Attributes:\n"
                    "%s"
                    "diff with previous = %.3f\n"
                    "vector diff = %.3f",
                    pointInfo, posDiff.length(), posDiff);
        }
        else
        {
            g_logger.info("Selected Point Attributes:\n"
                    "%s",
                    pointInfo);
        }
        // Snap cursor /and/ camera to new position
        // TODO: Decouple these, but in a sensible way
        m_cursorPos = newPos;
        m_camera.setCenter(newPos);
        m_prevCursorSnap = newPos;
    }
}

void View3D::mouseMoveEvent(QMouseEvent* event)
{
    if (m_mouseButton == Qt::MiddleButton)
        return;
    bool zooming = m_mouseButton == Qt::RightButton;
    if (event->modifiers() & Qt::ControlModifier)
    {
        m_cursorPos = m_camera.mouseMovePoint(m_cursorPos,
                                              event->pos() - m_prevMousePos,
                                              zooming);
        restartRender();
    }
    else
        m_camera.mouseDrag(m_prevMousePos, event->pos(), zooming);

    m_prevMousePos = event->pos();
}


void View3D::wheelEvent(QWheelEvent* event)
{
    // Translate mouse wheel events into vertical dragging for simplicity.
    m_camera.mouseDrag(QPoint(0,0), QPoint(0, -event->angleDelta().y()/2), true);
}


void View3D::keyPressEvent(QKeyEvent *event)
{
    // Centre camera on current cursor location
    if (event->key() == Qt::Key_C)
    {
        m_camera.setCenter(m_cursorPos);
    }

    if (m_dataSet)
    {
        switch (event->key())
        {
        case Qt::Key_1:
            m_dataSet->selectIndex(0);
            break;
        case Qt::Key_2:
            m_dataSet->selectIndex(1);
            break;
        case Qt::Key_3:
            m_dataSet->selectIndex(2);
            break;
        case Qt::Key_4:
            m_dataSet->selectIndex(3);
            break;
        case Qt::Key_5:
            m_dataSet->selectIndex(4);
            break;
        case Qt::Key_6:
            m_dataSet->selectIndex(5);
            break;
        case Qt::Key_7:
            m_dataSet->selectIndex(6);
            break;
        case Qt::Key_8:
            m_dataSet->selectIndex(7);
            break;
        case Qt::Key_9:
            m_dataSet->selectIndex(8);
            break;
        case Qt::Key_0:
            m_dataSet->selectIndex(9);
            break;
        }
    }

    event->ignore();
}

void View3D::initCursor(float cursorRadius, float centerPointRadius)
{
    float r1 = cursorRadius;
    float r2 = r1 + cursorRadius;

    float s = 1.0;
    float cursorPoints[] = {  r1*s,  0.0,  0.0,
                              r2*s,  0.0,  0.0,
                             -r1*s,  0.0,  0.0,
                             -r2*s,  0.0,  0.0,
                               0.0,  r1*s, 0.0,
                               0.0,  r2*s, 0.0,
                               0.0, -r1*s, 0.0,
                               0.0, -r2*s, 0.0  };

    m_cursorShader.reset(new ShaderProgram());
    bool cursor_shader_init = m_cursorShader->setShaderFromSourceFile("shaders:cursor.glsl");

    if (!cursor_shader_init)
    {
        g_logger.error("Could not read cursor shader.");
        return;
    }

    glGenVertexArrays(1, &m_cursorVertexArray);
    glBindVertexArray(m_cursorVertexArray);

    GlBuffer positionBuffer;
    positionBuffer.bind(GL_ARRAY_BUFFER);
    glBufferData(GL_ARRAY_BUFFER, (3) * 8 * sizeof(float), cursorPoints, GL_STATIC_DRAW);

    GLint positionAttribute = glGetAttribLocation(m_cursorShader->shaderProgram().programId(), "position");
    glVertexAttribPointer(positionAttribute, 3, GL_FLOAT, GL_FALSE, sizeof(float)*(3), (const GLvoid *)0);
    glEnableVertexAttribArray(positionAttribute);

    glBindVertexArray(0);
}

/// Draw the 3D cursor
void View3D::drawCursor(const TransformState& transStateIn, const V3d& cursorPos, float centerPointRadius) const
{
    glCheckError();

    V3d offset = transStateIn.cameraPos();
    TransformState transState = transStateIn.translate(offset);
    V3d relCursor = cursorPos - offset;

    // Cull if behind camera
    if ((relCursor * transState.modelViewMatrix).z > 0)
        return;

    // Find position of cursor in screen space
    V3d screenP3 = relCursor * transState.modelViewMatrix * transState.projMatrix;
    // Position in ortho coord system
    V2f p2 = 0.5f * V2f(width(), height()) * (V2f(screenP3.x, screenP3.y) + V2f(1.0f));

    // Draw cursor
    if (m_cursorShader->isValid())
    {
        QOpenGLShaderProgram& cursorShader = m_cursorShader->shaderProgram();
        // shader
        cursorShader.bind();
        // vertex array
        glBindVertexArray(m_cursorVertexArray);

        transState.projMatrix.makeIdentity();
        transState.setOrthoProjection(0, width(), 0, height(), 0, 1);
        transState.modelViewMatrix.makeIdentity();

        if (centerPointRadius > 0)
        {
            // fake drawing of white point through scaling ...
            //
            TransformState pointState = transState.translate( V3d(p2.x, p2.y, 0) );
            pointState = pointState.scale( V3d(0.0,0.0,0.0) );
            glLineWidth(1);
            cursorShader.setUniformValue("color", 1.0f, 1.0f, 1.0f, 1.0f);
            pointState.setUniforms(cursorShader.programId());
            glDrawArrays( GL_POINTS, 0, 1 );
        }

        // Now draw a 2D overlay over the 3D scene to allow user to pinpoint the
        // cursor, even when when it's behind something.
        glDisable(GL_DEPTH_TEST);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_LINE_SMOOTH);

        glLineWidth(1); // this won't work anymore for values larger than 1 (2.0f);

        // draw white lines
        transState = transState.translate( V3d(p2.x, p2.y, 0) );
        cursorShader.setUniformValue("color", 1.0f, 1.0f, 1.0f, 1.0f);
        transState.setUniforms(cursorShader.programId());
        glDrawArrays( GL_LINES, 0, 8 );

        // draw black lines
        transState = transState.rotate( V4d(0,0,1,0.785398) ); //45 deg
        cursorShader.setUniformValue("color", 0.0f, 0.0f, 0.0f, 1.0f);
        transState.setUniforms(cursorShader.programId());
        glDrawArrays( GL_LINES, 0, 8 );

        glBindVertexArray(0);
    }

    glCheckError();
}

void View3D::initAxes()
{
    glCheckError();

    m_drawAxesBackground = std::make_unique<QOpenGLTexture>(QImage(":/resource/axes.png"), QOpenGLTexture::DontGenerateMipMaps);
    m_drawAxesLabelX = std::make_unique<QOpenGLTexture>(QImage(":/resource/x.png"), QOpenGLTexture::DontGenerateMipMaps);
    m_drawAxesLabelY = std::make_unique<QOpenGLTexture>(QImage(":/resource/y.png"), QOpenGLTexture::DontGenerateMipMaps);
    m_drawAxesLabelZ = std::make_unique<QOpenGLTexture>(QImage(":/resource/z.png"), QOpenGLTexture::DontGenerateMipMaps);

    objectLabel(GL_TEXTURE, m_drawAxesBackground->textureId(), "axes.png");
    objectLabel(GL_TEXTURE, m_drawAxesLabelX->textureId(), "x.png");
    objectLabel(GL_TEXTURE, m_drawAxesLabelY->textureId(), "y.png");
    objectLabel(GL_TEXTURE, m_drawAxesLabelZ->textureId(), "z.png");

    m_axesBackgroundShader.reset(new ShaderProgram("axes_quad"));
    bool axesb_shader_init = m_axesBackgroundShader->setShaderFromSourceFile("shaders:axes_quad.glsl");
    m_axesLabelShader.reset(new ShaderProgram());
    bool axesl_shader_init = m_axesLabelShader->setShaderFromSourceFile("shaders:axes_label.glsl");
    m_axesShader.reset(new ShaderProgram());
    bool axes_shader_init = m_axesShader->setShaderFromSourceFile("shaders:axes_lines.glsl");

    if (!axes_shader_init || !axesb_shader_init || !axesl_shader_init)
    {
        g_logger.error("Could not read axes shaders");
        return;
    }

    const GLfloat w = 64.0;    // Width of axes widget
    const GLfloat o = 8.0;     // Axes widget offset in x and y
    const GLfloat transparency = 0.5f;

    // Array buffer for axes widget background

    float axesQuad[] = { o  , o,  0.0f, 0.0f,
                         o+w, o,  1.0f, 0.0f,
                         o+w, o+w,1.0f, 1.0f,
                         o  , o,  0.0f, 0.0f,
                         o+w, o+w,1.0f, 1.0f,
                         o  , o+w,0.0f, 1.0f };

    glGenVertexArrays(1, &m_quadVertexArray);
    glBindVertexArray(m_quadVertexArray);

    GLuint axesQuadVertexBuffer;
    glGenBuffers(1, &axesQuadVertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, axesQuadVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, (2 + 2) * 6 * sizeof(float), axesQuad, GL_STATIC_DRAW);

    GLint positionAttribute = glGetAttribLocation(m_axesBackgroundShader->shaderProgram().programId(), "position");

    glVertexAttribPointer(positionAttribute, 2, GL_FLOAT, GL_FALSE, sizeof(float)*(2 + 2), (const GLvoid *)0);
    glEnableVertexAttribArray(positionAttribute);

    GLint texCoordAttribute = glGetAttribLocation(m_axesBackgroundShader->shaderProgram().programId(), "texCoord");

    glVertexAttribPointer(texCoordAttribute, 2, GL_FLOAT, GL_FALSE, sizeof(float)*(2 + 2), (const GLvoid *)(sizeof(float)*2));
    glEnableVertexAttribArray(texCoordAttribute);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);


    const GLint l = 16;     // Label is 16 pixels wide

    float labelQuad[] = { -l/2, l/2, 0.0f, 0.0f,
                           l/2, l/2, 1.0f, 0.0f,
                           l/2,-l/2, 1.0f, 1.0f,
                          -l/2, l/2, 0.0f, 0.0f,
                           l/2,-l/2, 1.0f, 1.0f,
                          -l/2,-l/2, 0.0f, 1.0f };

    glGenVertexArrays(1, &m_quadLabelVertexArray);
    glBindVertexArray(m_quadLabelVertexArray);

    GLuint labelQuadVertexBuffer;
    glGenBuffers(1, &labelQuadVertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, labelQuadVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, (2 + 2) * 6 * sizeof(float), labelQuad, GL_STATIC_DRAW);

    positionAttribute = glGetAttribLocation(m_axesLabelShader->shaderProgram().programId(), "position");

    glVertexAttribPointer(positionAttribute, 2, GL_FLOAT, GL_FALSE, sizeof(float)*(2 + 2), (const GLvoid *)0);
    glEnableVertexAttribArray(positionAttribute);

    texCoordAttribute = glGetAttribLocation(m_axesLabelShader->shaderProgram().programId(), "texCoord");

    glVertexAttribPointer(texCoordAttribute, 2, GL_FLOAT, GL_FALSE, sizeof(float)*(2 + 2), (const GLvoid *)(sizeof(float)*2));
    glEnableVertexAttribArray(texCoordAttribute);
    glBindVertexArray(0);


    // Center of axis overlay
    const V3f center(o+w/2,o+w/2,0.0); //(0.0,0.0,0.0); //

    // color tint
    const float c = 0.8f;
    const float d = 0.5f;
    const float t = 0.5f*(1+transparency);
    const float r = 0.6f;  // 60% towards edge of circle

    const float le = r*w/2; //1.0f;  // we'll scale this later to match with the 60% sizing

    // just make up some lines for now ... this has to be updated later on ... unless we want to use rotations ?
    float axesLines[] = { center.x, center.y, center.z, c,d,d,t,
                          center.x+le, center.y, center.z, c,d,d,t,
                          center.x, center.y, center.z, d,c,d,t,
                          center.x, center.y+le, center.z, d,c,d,t,
                          center.x, center.y, center.z, d,d,c,t,
                          center.x, center.y, center.z+le, d,d,c,t, };

    glGenVertexArrays(1, &m_axesVertexArray);
    glBindVertexArray(m_axesVertexArray);

    GLuint axesLinesVertexBuffer;
    glGenBuffers(1, &axesLinesVertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, axesLinesVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, (3 + 4) * 6 * sizeof(float), axesLines, GL_DYNAMIC_DRAW); // make this a STREAM buffer ?

    positionAttribute = glGetAttribLocation(m_axesShader->shaderProgram().programId(), "position");

    glVertexAttribPointer(positionAttribute, 3, GL_FLOAT, GL_FALSE, sizeof(float)*(3 + 4), (const GLvoid *)0);
    glEnableVertexAttribArray(positionAttribute);

    GLint colorAttribute = glGetAttribLocation(m_axesShader->shaderProgram().programId(), "color");

    glVertexAttribPointer(colorAttribute, 4, GL_FLOAT, GL_FALSE, sizeof(float)*(3 + 4), (const GLvoid *)(sizeof(float)*3));
    glEnableVertexAttribArray(colorAttribute);

    //glBindFragDataLocation(m_axesShader->shaderProgram().programId(), 0, "fragColor");

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void View3D::drawAxes()
{
    glCheckError();

    glDisable(GL_DEPTH_TEST);

    TransformState projState(Imath::V2i(width(), height()),
                             Imath::M44d(),
                             Imath::M44d());

    projState.projMatrix.makeIdentity();
    projState.modelViewMatrix.makeIdentity();
    projState.setOrthoProjection(0, width(), 0, height(), 0, 1);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture(GL_TEXTURE0);

    const GLint w = 64;    // Width of axes widget
    const GLint o = 8;     // Axes widget offset in x and y

    // Center of axis overlay
    const V3d center(o + w/2, o + w/2, 0.0);

    // Draw background texture

    if (m_axesBackgroundShader->isValid())
    {
        QOpenGLShaderProgram& axesBackgroundShader = m_axesBackgroundShader->shaderProgram();
        axesBackgroundShader.bind();
        axesBackgroundShader.setUniformValue("texture0", 0);
        projState.setUniforms(axesBackgroundShader.programId());

        m_drawAxesBackground->bind();
        glBindVertexArray(m_quadVertexArray);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

    // Draw axes
    if (m_axesShader->isValid())
    {
        QOpenGLShaderProgram& axesShader = m_axesShader->shaderProgram();
        axesShader.bind();
        axesShader.setUniformValue("center", center.x, center.y, center.z);

        projState.modelViewMatrix = m_camera.rotationMatrix();
        projState.setUniforms(axesShader.programId());

        glBindVertexArray(m_axesVertexArray);
            glLineWidth(1); // this won't work anymore for values larger than 1 (4.0f);
            glDrawArrays(GL_LINES, 0, 6);
        glBindVertexArray(0);
    }

    // Draw Labels

    assert(m_axesLabelShader->isValid());
    if (m_axesLabelShader->isValid())
    {
        const double r = 0.8;   // 80% towards edge of circle

        // TODO: check if we should do this again:
        // Note that V3d -> V3i (double to integer precision)
        // conversion is intentionally snapping the label to
        // integer co-ordinates to eliminate subpixel aliasing
        // artifacts.  This is also the reason that matrix
        // transformations are not being used for this.

        const V3d px = V3d(1.0, 0.0, 0.0) * r * w/2;
        const V3d py = V3d(0.0, 1.0, 0.0) * r * w/2;
        const V3d pz = V3d(0.0, 0.0, 1.0) * r * w/2;

        // shader
        QOpenGLShaderProgram& axesLabelShader = m_axesLabelShader->shaderProgram();
        assert(axesLabelShader.isLinked());
        axesLabelShader.bind();
        axesLabelShader.setUniformValue("texture0", 0);
        projState.setUniforms(axesLabelShader.programId());
        axesLabelShader.setUniformValue("center", center.x, center.y, center.z);

        glBindVertexArray(m_quadLabelVertexArray);

            {
                axesLabelShader.setUniformValue("offset", px.x, px.y, px.z);
                m_drawAxesLabelX->bind();
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }

            {
                axesLabelShader.setUniformValue("offset", py.x, py.y, py.z);
                m_drawAxesLabelY->bind();
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }

            {
                axesLabelShader.setUniformValue("offset", pz.x, pz.y, pz.z);
                m_drawAxesLabelZ->bind();
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }

        glBindVertexArray(0);
    }

    glCheckError();
}


void View3D::initGrid(const float scale)
{
    m_gridShader.reset(new ShaderProgram());
    bool grid_shader_init = m_gridShader->setShaderFromSourceFile("shaders:grid.glsl");

    if (!grid_shader_init)
    {
        g_logger.error("Could not read grid shader.");
        return;
    }

    float gridLines[] = { -0.5f*scale, -0.5f*scale, 0.0f,  0.5f*scale, -0.5f*scale, 0.0f,
                          -0.5f*scale, -0.4f*scale, 0.0f,  0.5f*scale, -0.4f*scale, 0.0f,
                          -0.5f*scale, -0.3f*scale, 0.0f,  0.5f*scale, -0.3f*scale, 0.0f,
                          -0.5f*scale, -0.2f*scale, 0.0f,  0.5f*scale, -0.2f*scale, 0.0f,
                          -0.5f*scale, -0.1f*scale, 0.0f,  0.5f*scale, -0.1f*scale, 0.0f,
                          -0.5f*scale,  0.0f,       0.0f,  0.5f*scale,  0.0f,       0.0f,
                          -0.5f*scale,  0.1f*scale, 0.0f,  0.5f*scale,  0.1f*scale, 0.0f,
                          -0.5f*scale,  0.2f*scale, 0.0f,  0.5f*scale,  0.2f*scale, 0.0f,
                          -0.5f*scale,  0.3f*scale, 0.0f,  0.5f*scale,  0.3f*scale, 0.0f,
                          -0.5f*scale,  0.4f*scale, 0.0f,  0.5f*scale,  0.4f*scale, 0.0f,
                          -0.5f*scale,  0.5f*scale, 0.0f,  0.5f*scale,  0.5f*scale, 0.0f,

                          -0.5f*scale, -0.5f*scale, 0.0f, -0.5f*scale,  0.5f*scale, 0.0f,
                          -0.4f*scale, -0.5f*scale, 0.0f, -0.4f*scale,  0.5f*scale, 0.0f,
                          -0.3f*scale, -0.5f*scale, 0.0f, -0.3f*scale,  0.5f*scale, 0.0f,
                          -0.2f*scale, -0.5f*scale, 0.0f, -0.2f*scale,  0.5f*scale, 0.0f,
                          -0.1f*scale, -0.5f*scale, 0.0f, -0.1f*scale,  0.5f*scale, 0.0f,
                           0.0f,       -0.5f*scale, 0.0f,  0.0f,        0.5f*scale, 0.0f,
                           0.1f*scale, -0.5f*scale, 0.0f,  0.1f*scale,  0.5f*scale, 0.0f,
                           0.2f*scale, -0.5f*scale, 0.0f,  0.2f*scale,  0.5f*scale, 0.0f,
                           0.3f*scale, -0.5f*scale, 0.0f,  0.3f*scale,  0.5f*scale, 0.0f,
                           0.4f*scale, -0.5f*scale, 0.0f,  0.4f*scale,  0.5f*scale, 0.0f,
                           0.5f*scale, -0.5f*scale, 0.0f,  0.5f*scale,  0.5f*scale, 0.0f,
                            };

    glGenVertexArrays(1, &m_gridVertexArray);
    glBindVertexArray(m_gridVertexArray);

    GLuint gridVertexBuffer;
    glGenBuffers(1, &gridVertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, gridVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, (22 * 2) * 3 * sizeof(float), gridLines, GL_STATIC_DRAW);

    GLint positionAttribute = glGetAttribLocation(m_gridShader->shaderProgram().programId(), "position");

    glVertexAttribPointer(positionAttribute, 3, GL_FLOAT, GL_FALSE, sizeof(float)*(3), (const GLvoid *)0);
    glEnableVertexAttribArray(positionAttribute);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void View3D::drawGrid() const
{
    glCheckError();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    TransformState transState(Imath::V2i(width(), height()),
                              m_camera.projectionMatrix(),
                              m_camera.viewMatrix());

    // Draw grid
    if (m_gridShader->isValid())
    {
        QOpenGLShaderProgram &gridShader = m_gridShader->shaderProgram();
        // shader
        gridShader.bind();
        // vertex buffer
        glBindVertexArray(m_gridVertexArray);
        // matrix stack
        transState.setUniforms(gridShader.programId());
        // draw
        glLineWidth(1); // this won't work anymore for values larger than 1 (2.0f);
        glDrawArrays(GL_LINES, 0, (22 * 2));
        // do NOT release shader, this is no longer supported in OpenGL 3.2
        glBindVertexArray(0);
    }

    glCheckError();
}


/// Draw point cloud
DrawCount View3D::drawPoints(const TransformState& transState,
                             const std::vector<const Geometry*>& geoms,
                             double quality, bool incrementalDraw)
{
    glCheckError();

    DrawCount totDrawCount;
    if (geoms.empty())
    {
        return DrawCount();
    }

    if (!m_shaderProgram->isValid())
    {
        return DrawCount();
    }

    double dPR = getDevicePixelRatio();

    m_enable->enableOrDisable();
    glStencilFunc(GL_EQUAL, 0, 255);
    glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);

    // Draw points
    QOpenGLShaderProgram& prog = m_shaderProgram->shaderProgram();
    prog.bind();
    m_shaderProgram->setUniforms();
    QModelIndexList selection = m_selectionModel->selectedRows();
    for (size_t i = 0; i < geoms.size(); ++i)
    {
        const Geometry& geom = *geoms[i];
        if (!geom.pointCount())
        {
            continue;
        }
        V3f relCursor = m_cursorPos - geom.offset();
        prog.setUniformValue("cursorPos", relCursor.x, relCursor.y, relCursor.z);
        prog.setUniformValue("fileNumber", (GLint)(selection[(int)i].row() + 1));
        prog.setUniformValue("pointPixelScale", (GLfloat)(0.5*width()*dPR*m_camera.projectionMatrix()[0][0]));
        totDrawCount += geom.drawPoints(prog, transState, quality, incrementalDraw);
    }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_VERTEX_PROGRAM_POINT_SIZE);

    glCheckError();
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
    V3d refPos = 0.3*m_camera.position() + 0.7*m_camera.center();
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
    EllipticalDist distFunc(pos, viewDir, normalScaling);
    double nearestDist = DBL_MAX;
    // Snap cursor to position of closest point and center on it
    QModelIndexList sel = m_selectionModel->selectedRows();
    for (int i = 0; i < sel.size(); ++i)
    {
        int geomIdx = sel[i].row();
        V3d pickedVertex;
        double dist = 0;
        std::string info;
        if (m_geometries->get()[geomIdx]->pickVertex(cameraPos, distFunc,
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
    for (int i = 0; i < sel.size(); ++i)
        geoms.push_back(geomAll[sel[i].row()].get());
    return geoms;
}

void View3D::readSettings(const QSettings& settings)
{
    m_drawBoundingBoxes = settings.value("boundingBoxes", m_drawBoundingBoxes).toBool();
    m_drawCursor        = settings.value("cursor", m_drawCursor).toBool();
    m_drawAxes          = settings.value("axes", m_drawAxes).toBool();
    m_drawGrid          = settings.value("grid", m_drawGrid).toBool();
    m_drawAnnotations   = settings.value("annotations", m_drawAnnotations).toBool();
    m_backgroundColor   = settings.value("background", m_backgroundColor).value<QColor>();

    m_boundingBoxAction->setChecked(m_drawBoundingBoxes);
    m_cursorAction->setChecked(m_drawCursor);
    m_axesAction->setChecked(m_drawAxes);
    m_gridAction->setChecked(m_drawGrid);
    m_annotationAction->setChecked(m_drawAnnotations);
}

void View3D::writeSettings(QSettings& settings) const
{
    settings.setValue("boundingBoxes", m_drawBoundingBoxes);
    settings.setValue("cursor", m_drawCursor);
    settings.setValue("axes", m_drawAxes);
    settings.setValue("grid", m_drawGrid);
    settings.setValue("annotations", m_drawAnnotations);
    settings.setValue("background", QVariant(m_backgroundColor));
}

// vi: set et:
