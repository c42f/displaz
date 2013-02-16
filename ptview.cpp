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

//#define GL_GLEXT_PROTOTYPES
// Hack: define gl flags which are normally done by GLEW...
// perhaps should bring glew back as a dependency...
#ifndef GL_VERTEX_PROGRAM_POINT_SIZE
#   define GL_VERTEX_PROGRAM_POINT_SIZE 0x8642
#endif
#ifndef GL_POINT_SPRITE
#   define GL_POINT_SPRITE 0x8861
#endif

#include <QtGui/QKeyEvent>
//#include <QtOpenGL/QGLBuffer>

#ifdef _WIN32
#   include <ImathVec.h>
#   include <ImathMatrix.h>
#else
#   include <OpenEXR/ImathVec.h>
#   include <OpenEXR/ImathBox.h>
#   include <OpenEXR/ImathMatrix.h>
#endif

#include "ptview.h"
#include "tinyformat.h"


//----------------------------------------------------------------------
//#include <OpenEXR/ImathGL.h>
// Utilities for OpenEXR / OpenGL interoperability.
//
// Technically we could use the stuff from ImathGL instead here, but it has
// portability problems for OSX due to how it includes gl.h (this is an
// libilmbase bug, at least up until 1.0.2)
inline void glTranslate(const Imath::V3f& v)
{
    glTranslatef(v.x, v.y, v.z);
}

inline void glVertex(const Imath::V3f& v)
{
    glVertex3f(v.x, v.y, v.z);
}

inline void glVertex(const Imath::V2f& v)
{
    glVertex2f(v.x, v.y);
}

inline void glColor(const Imath::C3f& c)
{
    glColor3f(c.x, c.y, c.z);
}

inline void glLoadMatrix(const Imath::M44f& m)
{
    glLoadMatrixf((GLfloat*)m[0]);
}


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


//------------------------------------------------------------------------------
PointView::PointView(QWidget *parent)
    : QGLWidget(parent),
    m_camera(false, false),
    m_lastPos(0,0),
    m_zooming(false),
    m_cursorPos(0),
    m_prevCursorSnap(0),
    m_drawOffset(0),
    m_backgroundColor(60, 50, 50),
    m_drawBoundingBoxes(true),
    m_shaderProgram(),
    m_points(),
    m_maxPointCount(10000000)
{
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    m_camera.setClipFar(FLT_MAX);
    // Don't use signals/slots for handling view changes... this seems to
    // introduce a bit of extra lag for slowly drawing scenes (?)
    //connect(&m_camera, SIGNAL(projectionChanged()), this, SLOT(updateGL()));
    //connect(&m_camera, SIGNAL(viewChanged()), this, SLOT(updateGL()));

    makeCurrent();
    m_shaderProgram.reset(new ShaderProgram(context()));
    connect(m_shaderProgram.get(), SIGNAL(uniformValuesChanged()),
            this, SLOT(updateGL()));
    connect(m_shaderProgram.get(), SIGNAL(shaderChanged()),
            this, SLOT(updateGL()));
}

PointView::~PointView() { }


void PointView::loadPointFilesImpl(PointArrayVec& pointArrays,
                                   const QStringList& fileNames)
{
    emit fileLoadStarted();
    pointArrays.clear();
    size_t maxCount = m_maxPointCount / fileNames.size();
    QStringList successfullyLoaded;
    for(int i = 0; i < fileNames.size(); ++i)
    {
        std::unique_ptr<PointArray> points(new PointArray());
        connect(points.get(), SIGNAL(pointsLoaded(int)),
                this, SIGNAL(pointsLoaded(int)));
        if(points->loadPointFile(fileNames[i], maxCount) && !points->empty())
        {
            pointArrays.push_back(std::move(points));
            successfullyLoaded.push_back(fileNames[i]);
            emit pointFilesLoaded(successfullyLoaded);
        }
    }
    emit fileLoadFinished();
}


void PointView::loadPointFiles(const QStringList& fileNames)
{
    loadPointFilesImpl(m_points, fileNames);
    if(!m_points.empty())
    {
        m_cursorPos = m_points[0]->centroid();
        m_drawOffset = m_points[0]->offset();
        m_camera.setCenter(exr2qt(m_cursorPos - m_drawOffset));
        double diag = (m_points[0]->boundingBox().max -
                       m_points[0]->boundingBox().min).length();
        m_camera.setEyeToCenterDistance(diag*0.7);
    }
    updateGL();
}


void PointView::reloadPointFiles()
{
    m_drawOffset = m_points[0]->offset();
    QStringList fileNames;
    for(size_t i = 0; i < m_points.size(); ++i)
        fileNames << m_points[i]->fileName();
    loadPointFilesImpl(m_points, fileNames);
    if(!m_points.empty())
    {
        m_drawOffset = m_points[0]->offset();
        m_camera.setCenter(exr2qt(m_cursorPos - m_drawOffset));
    }
    updateGL();
}


void PointView::setBackground(QColor col)
{
    m_backgroundColor = col;
    updateGL();
}


void PointView::toggleDrawBoundingBoxes()
{
    m_drawBoundingBoxes = !m_drawBoundingBoxes;
}

void PointView::toggleCameraMode()
{
    m_camera.setTrackballInteraction(!m_camera.trackballInteraction());
}


void PointView::setMaxPointCount(size_t maxPointCount)
{
    m_maxPointCount = maxPointCount;
}


void PointView::initializeGL()
{
}


void PointView::resizeGL(int w, int h)
{
    // Draw on full window
    glViewport(0, 0, w, h);
    m_camera.setViewport(geometry());
}


void PointView::paintGL()
{
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
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Draw geometry
    if (!m_points.empty())
    {
        for(size_t i = 0; i < m_points.size(); ++i)
            drawPoints(*m_points[i], i, m_drawOffset);
    }
    // Draw overlay stuff, including cursor position.
    drawCursor(m_cursorPos - m_drawOffset);
}


void PointView::mousePressEvent(QMouseEvent* event)
{
    m_zooming = event->button() == Qt::RightButton;
    m_lastPos = event->pos();
}


void PointView::mouseMoveEvent(QMouseEvent* event)
{
    if(event->modifiers() & Qt::ControlModifier)
    {
        m_cursorPos = qt2exr(
            m_camera.mouseMovePoint(exr2qt(m_cursorPos - m_drawOffset),
                                    event->pos() - m_lastPos,
                                    m_zooming) ) + m_drawOffset;
        updateGL();
    }
    else
    {
        m_camera.mouseDrag(m_lastPos, event->pos(), m_zooming);
        updateGL();
    }
    m_lastPos = event->pos();
}


void PointView::wheelEvent(QWheelEvent* event)
{
    // Translate mouse wheel events into vertical dragging for simplicity.
    m_camera.mouseDrag(QPoint(0,0), QPoint(0, -event->delta()/2), true);
    updateGL();
}


void PointView::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_C)
    {
        // Centre camera on current cursor location
        m_camera.setCenter(exr2qt(m_cursorPos - m_drawOffset));
        updateGL();
    }
    else if(event->key() == Qt::Key_S)
    {
        if(m_points.empty())
            return;
        // Snap cursor to position of closest point and center on it
        V3d newPos(0);
        size_t nearestIdx = 0;
        size_t nearestCloudIdx = 0;
        double nearestDist = DBL_MAX;
        for(size_t i = 0; i < m_points.size(); ++i)
        {
            if(m_points[i]->empty())
                continue;
            double dist = 0;
            size_t idx = m_points[i]->closestPoint(m_cursorPos, &dist);
            if(dist < nearestDist)
            {
                nearestDist = dist;
                nearestIdx = idx;
                nearestCloudIdx = i;
            }
        }
        newPos = m_points[nearestCloudIdx]->absoluteP(nearestIdx);
        V3d posDiff = m_cursorPos - m_prevCursorSnap;
        tfm::printf("Point %d: (%.3f, %.3f, %.3f) [diff with previous = (%.3f, %.3f, %.3f)]\n", nearestIdx,
                    newPos.x, newPos.y, newPos.z, posDiff.x, posDiff.y, posDiff.z);
        m_cursorPos = newPos;
        m_prevCursorSnap = newPos;
        m_camera.setCenter(exr2qt(newPos - m_drawOffset));
        updateGL();
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
    if(screenP3.z < 0)
        return; // Cull if behind the camera.  TODO: doesn't work quite right

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


static void drawBoundingBox(const Imath::Box3d& bbox)
{
    double xbnd[2] = {bbox.min.x, bbox.max.x};
    double ybnd[2] = {bbox.min.y, bbox.max.y};
    double zbnd[2] = {bbox.min.z, bbox.max.z};
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor3f(1,1,1);
    glLineWidth(1);
    glBegin(GL_LINES);
    for (int i = 0; i < 2; ++i)
    for (int j = 0; j < 2; ++j)
    {
        glVertex3f(xbnd[i], ybnd[j], zbnd[0]);
        glVertex3f(xbnd[i], ybnd[j], zbnd[1]);
    }
    for (int i = 0; i < 2; ++i)
    for (int j = 0; j < 2; ++j)
    {
        glVertex3f(xbnd[i], ybnd[0], zbnd[j]);
        glVertex3f(xbnd[i], ybnd[1], zbnd[j]);
    }
    for (int i = 0; i < 2; ++i)
    for (int j = 0; j < 2; ++j)
    {
        glVertex3f(xbnd[0], ybnd[i], zbnd[j]);
        glVertex3f(xbnd[1], ybnd[i], zbnd[j]);
    }
    glEnd();
    glDisable(GL_BLEND);
}


/// Draw point cloud
void PointView::drawPoints(const PointArray& points,
                           int fileNumber, const V3d& drawOffset) const
{
    if(points.empty())
        return;
    if(m_drawBoundingBoxes)
    {
        // Draw bounding box
        Imath::Box3d bbox = points.boundingBox();
        bbox.min -= drawOffset;
        bbox.max -= drawOffset;
        drawBoundingBox(bbox);
    }
    glPushMatrix();
    V3d offset = points.offset() - drawOffset;
    glTranslatef(offset.x, offset.y, offset.z);

    glEnable(GL_POINT_SPRITE);
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    // Draw points
    QGLShaderProgram& prog = m_shaderProgram->shaderProgram();
    prog.bind();
    m_shaderProgram->setUniforms();
    //prog.setUniformValue("modelViewMatrix", ); // TODO
    //prog.setUniformValue("projectionMatrix", );
    V3f relCursor = m_cursorPos - points.offset();
    prog.setUniformValue("cursorPos", relCursor.x, relCursor.y, relCursor.z);
    prog.setUniformValue("fileNumber", fileNumber);
//    QGLBuffer intensityBuf(QGLBuffer::VertexBuffer);
//    intensityBuf.setUsagePattern(QGLBuffer::DynamicDraw);
//    intensityBuf.create();
//    intensityBuf.allocate(sizeof(float)*chunkSize);
//    intensityBuf.bind();
//    intensityBuf.write(0, points.intensity() + i, ndraw);
//    prog.setAttributeBuffer("intensity", GL_FLOAT, 0, 1);
//    intensityBuf.release();
    points.draw(prog, m_cursorPos);
    prog.release();
    glDisable(GL_VERTEX_PROGRAM_POINT_SIZE);
    glPopMatrix();
}


// vi: set et:
