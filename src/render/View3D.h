// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef DISPLAZ_VIEW3D_H_INCLUDED
#define DISPLAZ_VIEW3D_H_INCLUDED

#include <cmath>
#include <vector>
#include <memory>

#include "glutil.h"
#define QT_NO_OPENGL_ES_2


#include <QVector>
#include <QGLWidget>
#include <QModelIndex>

#include "DrawCostModel.h"
#include "InteractiveCamera.h"
#include "geometrycollection.h"
#include "Annotation.h"

class QGLShaderProgram;
class QItemSelectionModel;
class QTimer;
class QGLFormat;

class Enable;
class ShaderProgram;
struct TransformState;

//------------------------------------------------------------------------------
/// OpenGL-based viewer widget for point clouds
class View3D : public QGLWidget
{
    Q_OBJECT
    public:
        View3D(GeometryCollection* geometries, const QGLFormat& format, QWidget *parent = NULL);
        ~View3D();

        Enable& enable() const { return *m_enable; }

        /// Return shader used for displaying points
        ShaderProgram& shaderProgram() const { return *m_shaderProgram; }

        void setShaderParamsUIWidget(QWidget* widget);

        InteractiveCamera& camera() { return m_camera; }

        QColor background() const { return m_backgroundColor; }

        Imath::V3d cursorPos() const { return m_cursorPos; }

        /// Return current selection of loaded files
        const QItemSelectionModel* selectionModel() const { return m_selectionModel; }
        QItemSelectionModel* selectionModel() { return m_selectionModel; }
        void setSelectionModel(QItemSelectionModel* selectionModel);

        void addAnnotation(const QString& label, const QString& text,
                           const Imath::V3d& pos);

        /// Remove all anontations who's label matches the given QRegExp
        void removeAnnotations(const QRegExp& labelRegex);


    public slots:
        /// Set the backgroud color
        void setBackground(QColor col);
        void toggleDrawBoundingBoxes();
        void toggleDrawCursor();
        void toggleDrawAxes();
        void toggleDrawGrid();
        void toggleDrawAnnotations();
        void toggleCameraMode();
        /// Centre on loaded geometry file at the given index
        void centerOnGeometry(const QModelIndex& index);
        void centerOnPoint(const Imath::V3d& pos);
        void setExplicitCursorPos(const Imath::V3d& pos);

    protected:
        // Qt OpenGL callbacks
        void initializeGL();
        void resizeGL(int w, int h);
        void paintGL();

        // Qt event callbacks
        void mousePressEvent(QMouseEvent* event);
        void mouseMoveEvent(QMouseEvent* event);
        void wheelEvent(QWheelEvent* event);
        void keyPressEvent(QKeyEvent* event);

    private slots:
        void restartRender();
        void setupShaderParamUI();

        void geometryChanged();
        void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
        void geometryInserted(const QModelIndex&, int firstRow, int lastRow);

    private:
        double getDevicePixelRatio();
        void initializeGLGeometry(int begin, int end);

        void initCursor(float cursorRadius, float centerPointRadius);
        void drawCursor(const TransformState& transState, const V3d& P, float centerPointRadius) const;

        void initAxes();
        void drawAxes() const;

        void initGrid(const float scale);
        void drawGrid() const;

        void drawText(const QString& text);

        DrawCount drawPoints(const TransformState& transState,
                             const std::vector<const Geometry*>& geoms,
                             double quality, bool incrementalDraw);

        void drawMeshes(const TransformState& transState,
                        const std::vector<const Geometry*>& geoms) const;
        void drawAnnotations(const TransformState& transState,
                             int viewportPixelWidth,
                             int viewportPixelHeight) const;

        Imath::V3d guessClickPosition(const QPoint& clickPos);

        bool snapToGeometry(const Imath::V3d& pos, double normalScaling,
                            Imath::V3d* newPos, QString* pointInfo);

        void snapToPoint(const Imath::V3d& pos);
        std::vector<const Geometry*> selectedGeometry() const;

        /// Mouse-based camera positioning
        InteractiveCamera m_camera;
        QPoint m_prevMousePos;
        Qt::MouseButton m_mouseButton;
        bool m_middleButton;
        // if true, an explicit cursor position has been specified
        bool m_explicitCursorPos;
        /// Position of 3D cursor
        V3d m_cursorPos;
        V3d m_prevCursorSnap;
        /// Background color for drawing
        QColor m_backgroundColor;
        /// Option to draw bounding boxes of point clouds
        bool m_drawBoundingBoxes;
        bool m_drawCursor;
        bool m_drawAxes;
        bool m_drawGrid;
        bool m_drawAnnotations;
        /// If true, OpenGL initialization didn't work properly
        bool m_badOpenGL;
        /// Shader for point clouds
        std::unique_ptr<ShaderProgram> m_shaderProgram;
        std::unique_ptr<Enable>        m_enable;
        /// Shaders for polygonal geometry
        std::unique_ptr<ShaderProgram> m_meshFaceShader;
        std::unique_ptr<ShaderProgram> m_meshEdgeShader;
        /// Collection of geometries
        GeometryCollection* m_geometries;
        QItemSelectionModel* m_selectionModel;
        QVector<std::shared_ptr<Annotation>> m_annotations;
        /// UI widget for shader
        QWidget* m_shaderParamsUI;
        /// Timer for next incremental frame
        QTimer* m_incrementalFrameTimer;
        Framebuffer m_incrementalFramebuffer;
        bool m_incrementalDraw;
        /// Controller for amount of geometry to draw
        DrawCostModel m_drawCostModel;
        /// GL textures
        Texture m_drawAxesBackground;
        Texture m_drawAxesLabelX;
        Texture m_drawAxesLabelY;
        Texture m_drawAxesLabelZ;

        /// Shaders for interface geometry
        std::unique_ptr<ShaderProgram> m_cursorShader;
        std::unique_ptr<ShaderProgram> m_axesShader;
        std::unique_ptr<ShaderProgram> m_gridShader;
        std::unique_ptr<ShaderProgram> m_axesBackgroundShader;
        std::unique_ptr<ShaderProgram> m_axesLabelShader;
        std::unique_ptr<ShaderProgram> m_boundingBoxShader;
        std::unique_ptr<ShaderProgram> m_annotationShader;

        unsigned int m_cursorVertexArray;
        unsigned int m_axesVertexArray;
        unsigned int m_gridVertexArray;
        unsigned int m_quadVertexArray;
        unsigned int m_quadLabelVertexArray;

        double m_devicePixelRatio;
};



#endif // DISPLAZ_VIEW3D_H_INCLUDED

// vi: set et:
