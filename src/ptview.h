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

#ifndef DISPLAZ_PTVIEW_H_INCLUDED
#define DISPLAZ_PTVIEW_H_INCLUDED

#include <cmath>
#include <vector>
#include <memory>

#include <QtOpenGL/QGLWidget>

#include "interactivecamera.h"
#include "pointarray.h"

class QGLShaderProgram;
class QGLFramebufferObject;
class QTimer;

class ShaderProgram;
class TriMesh;
class LineSegments;

//------------------------------------------------------------------------------
/// OpenGL-based viewer widget for point clouds
class PointView : public QGLWidget
{
    Q_OBJECT

    public:
        PointView(QWidget *parent = NULL);
        ~PointView();

        /// Load geometry files from disk
        void loadFiles(const QStringList& fileNames);

        /// Reload current files from disk
        void reloadFiles();

        /// Return shader used for displaying points
        ShaderProgram& shaderProgram() const { return *m_shaderProgram; }

        void setShaderParamsUIWidget(QWidget* widget);

    public slots:
        /// Set the backgroud color
        void setBackground(QColor col);
        void setMaxPointCount(size_t maxPointCount);
        void toggleDrawBoundingBoxes();
        void toggleDrawPoints();
        void toggleDrawMeshes();
        void toggleCameraMode();

    signals:
        void fileLoadStarted();
        void fileLoadFinished();
        /// Emitted each time a file is loaded.  Contains current list of files.
        void pointFilesLoaded(QStringList files) const;
        /// Emitted at the start of a point loading step
        void loadStepStarted(QString stepDescription);
        /// Emitted as progress is made loading points
        void pointsLoaded(int percentLoaded) const;

    protected:
        // Qt OpenGL callbacks
        void initializeGL();
        void resizeGL(int w, int h);
        void paintGL();

        // Qt event callbacks
        void mousePressEvent(QMouseEvent* event);
        void mouseReleaseEvent(QMouseEvent* event);
        void mouseMoveEvent(QMouseEvent* event);
        void wheelEvent(QWheelEvent* event);
        void keyPressEvent(QKeyEvent* event);

    private slots:
        void restartRender();
        void setupShaderParamUI();

    private:
        typedef std::vector<std::unique_ptr<PointArray> > PointArrayVec;
        typedef std::vector<std::unique_ptr<TriMesh> > MeshVec;
        typedef std::vector<std::unique_ptr<LineSegments> > LineSegVec;
        void loadPointFilesImpl(PointArrayVec& pointArrays,
                                MeshVec& meshes, LineSegVec& lines,
                                const QStringList& fileNames);

        void drawCursor(const V3f& P) const;
        size_t drawPoints(const PointArrayVec& allPoints,
                          size_t numPointsToRender, bool incrementalDraw);
        void drawMesh(const TriMesh& mesh, const V3d& drawOffset) const;

        void snapCursorAndCentre(double normalScaling);

        /// Mouse-based camera positioning
        InteractiveCamera m_camera;
        QPoint m_prevMousePos;
        Qt::MouseButton m_mouseButton;
        bool m_middleButton;
        /// Position of 3D cursor
        V3d m_cursorPos;
        V3d m_prevCursorSnap;
        /// Offset used when drawing
        V3d m_drawOffset;
        /// Background color for drawing
        QColor m_backgroundColor;
        /// Option to draw bounding boxes of point clouds
        bool m_drawBoundingBoxes;
        bool m_drawPoints;
        bool m_drawMeshes;
        /// Shader for point clouds
        std::unique_ptr<ShaderProgram> m_shaderProgram;
        /// Shaders for polygonal geometry
        std::unique_ptr<ShaderProgram> m_meshFaceShader;
        std::unique_ptr<ShaderProgram> m_meshEdgeShader;
        /// Point cloud data
        PointArrayVec m_points;
        MeshVec m_meshes;
        LineSegVec m_lines;
        /// UI widget for shader
        QWidget* m_shaderParamsUI;
        /// Maximum desired number of points to load
        size_t m_maxPointCount;
        /// Timer for next incremental frame
        QTimer* m_incrementalFrameTimer;
        std::unique_ptr<QGLFramebufferObject> m_incrementalFramebuffer;
        bool m_incrementalDraw;
        /// Target for max total number of points to draw per frame
        size_t m_maxPointsPerFrame;
};



#endif // DISPLAZ_PTVIEW_H_INCLUDED

// vi: set et:
