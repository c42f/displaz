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

#ifndef DISPLAZ_VIEW3D_H_INCLUDED
#define DISPLAZ_VIEW3D_H_INCLUDED

#include <cmath>
#include <vector>
#include <memory>

#include <GL/glew.h>
#include <QGLWidget>
#include <QModelIndex>

#include "interactivecamera.h"
#include "geometrycollection.h"

class QGLShaderProgram;
class QGLFramebufferObject;
class QItemSelectionModel;
class QTimer;

class ShaderProgram;
struct TransformState;

//------------------------------------------------------------------------------
/// OpenGL-based viewer widget for point clouds
class View3D : public QGLWidget
{
    Q_OBJECT
    public:
        View3D(GeometryCollection* geometries, QWidget *parent = NULL);
        ~View3D();

        /// Return shader used for displaying points
        ShaderProgram& shaderProgram() const { return *m_shaderProgram; }

        void setShaderParamsUIWidget(QWidget* widget);

        InteractiveCamera& camera() { return m_camera; }

        QColor background() const { return m_backgroundColor; }

        /// Return current selection of loaded files
        const QItemSelectionModel* selectionModel() const { return m_selectionModel; }
        QItemSelectionModel* selectionModel() { return m_selectionModel; }
        void setSelectionModel(QItemSelectionModel* selectionModel);

    public slots:
        /// Set the backgroud color
        void setBackground(QColor col);
        void toggleDrawBoundingBoxes();
        void toggleDrawCursor();
        void toggleCameraMode();
        /// Centre on loaded geometry file at the given index
        void centreOnGeometry(const QModelIndex& index);

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

        void geometryChanged();
        void geometryInserted(const QModelIndex&, int firstRow, int lastRow);

    private:
        std::unique_ptr<QGLFramebufferObject> allocIncrementalFramebuffer(int w, int h) const;
        void drawCursor(const TransformState& transState, const V3f& P) const;
        size_t drawPoints(const TransformState& transState,
                          const GeometryCollection::GeometryVec& allPoints,
                          const QModelIndexList& selection,
                          size_t numPointsToRender, bool incrementalDraw);
        void drawMeshes(const TransformState& transState,
                        const GeometryCollection::GeometryVec& geoms,
                        const QModelIndexList& sel) const;

        Imath::V3d guessClickPosition(const QPoint& clickPos);

        Imath::V3d snapToGeometry(const Imath::V3d& pos, double normalScaling);

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
        bool m_drawCursor;
        /// If true, OpenGL initialization didn't work properly
        bool m_badOpenGL;
        /// Shader for point clouds
        std::unique_ptr<ShaderProgram> m_shaderProgram;
        /// Shaders for polygonal geometry
        std::unique_ptr<ShaderProgram> m_meshFaceShader;
        std::unique_ptr<ShaderProgram> m_meshEdgeShader;
        /// Collection of geometries
        GeometryCollection* m_geometries;
        QItemSelectionModel* m_selectionModel;
        /// UI widget for shader
        QWidget* m_shaderParamsUI;
        /// Timer for next incremental frame
        QTimer* m_incrementalFrameTimer;
        std::unique_ptr<QGLFramebufferObject> m_incrementalFramebuffer;
        bool m_incrementalDraw;
        /// Target for max total number of points to draw per frame
        size_t m_maxPointsPerFrame;
};



#endif // DISPLAZ_VIEW3D_H_INCLUDED

// vi: set et:
