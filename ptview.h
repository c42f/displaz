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

#ifdef _MSC_VER
#   define _USE_MATH_DEFINES
#endif
#include <cmath>
#include <vector>
#include <memory>

#include <QtOpenGL/QGLWidget>
#include <QtOpenGL/QGLShaderProgram>

#ifdef _WIN32
#include <ImathVec.h>
#include <ImathBox.h>
#include <ImathColor.h>
#else
#include <OpenEXR/ImathVec.h>
#include <OpenEXR/ImathBox.h>
#include <OpenEXR/ImathColor.h>
#endif

#include "interactivecamera.h"

using Imath::V3d;
using Imath::V3f;
using Imath::V2f;
using Imath::C3f;

//------------------------------------------------------------------------------
/// Container for points to be displayed in the PointView interface
class PointArrayModel : public QObject
{
    Q_OBJECT

    public:
        PointArrayModel();

        /// Load points from a file
        bool loadPointFile(QString fileName, size_t maxPointCount,
                           const C3f& color);

        QString fileName() const { return m_fileName; }

        /// Return the number of points
        size_t size() const { return m_npoints; }
        /// Return true when there are zero points
        bool empty() const { return m_npoints == 0; }

        /// Return point position
        ///
        /// Note that this is stored relative to the offset() of the point
        /// cloud to avoid loss of precision.
        const V3f* P() const { return m_P.get(); }

        V3d absoluteP(size_t idx) const { return V3d(m_P[idx]) + m_offset; }
        /// Return point color, or NULL if no color channel is present
        const C3f* color() const { return m_color.get(); }
        const float* intensity() const { return m_intensity.get(); }

        /// Return index of closest point to the given position
        ///
        /// Also return the euclidian distance to the nearest point if the
        /// input distance parameter is non-null.
        size_t closestPoint(V3d pos, double* distance = 0) const;

        /// Get a list of channel names which look like color channels
        QStringList colorChannels() { return m_colorChannelNames; }

        /// Return the centroid of the position data
        const V3d& centroid() const { return m_centroid; }

        /// Get bounding box
        const Imath::Box3d& boundingBox() const { return m_bbox; }

        /// Get the offset which should be added to P to get absolute position
        V3d offset() const { return m_offset; }

    signals:
        /// Emitted progress is made loading points
        void loadedPoints(double fractionOfTotal);

    private:
        QString m_fileName;
        QStringList m_colorChannelNames;
        size_t m_npoints;
        V3d m_offset;
        Imath::Box3d m_bbox;
        V3d m_centroid;
        std::unique_ptr<V3f[]> m_P;
        std::unique_ptr<float[]> m_intensity;
        std::unique_ptr<C3f[]> m_color;
};


//------------------------------------------------------------------------------
/// OpenGL-based viewer widget for point clouds (or more precisely, clouds of
/// disk-like surface elements).
class PointView : public QGLWidget
{
    Q_OBJECT

    public:
        PointView(QWidget *parent = NULL);

        /// Load a point cloud from a file
        void loadPointFiles(const QStringList& fileNames);
        void reloadPointFiles();

        /// Hint at an appropriate size
        QSize sizeHint() const;

    public slots:
        /// Set the backgroud color
        void setBackground(QColor col);
        void setMaxPointCount(size_t maxPointCount);
        void toggleDrawBoundingBoxes();
        void toggleCameraMode();
        void setPointSize(double size);
        void setExposure(double intensity);
        void setContrast(double power);
        void setSelector(int sel);

    signals:
        void pointFilesLoaded(QStringList files);

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

    private:
        void drawCursor(const V3f& P) const;
        void drawPoints(const PointArrayModel& points,
                        int fileNumber, const V3d& drawOffset) const;

        /// Mouse-based camera positioning
        InteractiveCamera m_camera;
        QPoint m_lastPos;
        bool m_zooming;
        /// Position of 3D cursor
        V3d m_cursorPos;
        V3d m_prevCursorSnap;
        /// Offset used when drawing
        V3d m_drawOffset;
        /// Background color for drawing
        QColor m_backgroundColor;
        bool m_drawBoundingBoxes;
        /// Shader parameters
        double m_pointSize;
        double m_exposure;
        double m_contrast;
        int m_selector;
        /// Shader programs
        QGLShaderProgram* m_pointShader;
        /// Point cloud data
        std::vector<std::unique_ptr<PointArrayModel> > m_points;
        size_t m_maxPointCount; ///< Maximum desired number of points to load
};



#endif // DISPLAZ_PTVIEW_H_INCLUDED

// vi: set et:
