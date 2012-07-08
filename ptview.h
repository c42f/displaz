// Copyright (C) 2001, Paul C. Gregory and the other authors and contributors
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
// (This is the New BSD license)

#ifndef AQSIS_PTVIEW_H_INCLUDED
#define AQSIS_PTVIEW_H_INCLUDED

#include <cmath>
#include <vector>

#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>

#include <QtGui/QMainWindow>
#include <QtOpenGL/QGLWidget>

#include <OpenEXR/ImathVec.h>
#include <OpenEXR/ImathColor.h>

#include "interactivecamera.h"

class QActionGroup;
class QSignalMapper;


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
        bool loadPointFile(const QString& fileName);

        /// Return the number of points
        size_t size() const { return m_npoints; }
        /// Return true when there are zero points
        bool empty() const { return m_npoints == 0; }

        /// Return point position
        const V3f* P() const { return m_P.get(); }
        /// Return point color, or NULL if no color channel is present
        const C3f* color() const { return m_color.get(); }

        /// Get a list of channel names which look like color channels
        QStringList colorChannels() { return m_colorChannelNames; }

        /// Set the channel name which the color() function returns data for
        void setColorChannel(const QString& name) {};

        /// Compute the centroid of the P data
        V3f centroid() const;

    private:
        QString m_fileName;
        QStringList m_colorChannelNames;
        size_t m_npoints;
        boost::shared_array<V3f> m_P;
        boost::shared_array<C3f> m_color;
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
        /// Set properties for rendering probe environment map
        void setProbeParams(int cubeFaceRes, float maxSolidAngle);

        /// Hint at an appropriate size
        QSize sizeHint() const;

    public slots:
        /// Set the backgroud color
        void setBackground(QColor col);
        void setColorChannel(QString channel);
        void toggleDrawAxes();

    signals:
        void colorChannelsChanged(QStringList channels);

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
        static void drawAxes();
        void drawCursor(const V3f& P) const;
        static void drawPoints(const PointArrayModel& points);

        /// Mouse-based camera positioning
        InteractiveCamera m_camera;
        QPoint m_lastPos;
        bool m_zooming;
        /// Position of 3D cursor
        V3f m_cursorPos;
        /// Light probe resolution
        int m_probeRes;
        float m_probeMaxSolidAngle;
        /// Background color for drawing
        QColor m_backgroundColor;
        bool m_drawAxes;
        /// Point cloud data
        std::vector<boost::shared_ptr<PointArrayModel> > m_points;
        V3f m_cloudCenter;
};


//------------------------------------------------------------------------------
/// Main window for point cloud viewer application
class PointViewerMainWindow : public QMainWindow
{
    Q_OBJECT

    public:
        PointViewerMainWindow(const QStringList& initialPointFileNames =
                              QStringList());

        PointView& pointView() { return *m_pointView; }

    protected:
        void keyReleaseEvent(QKeyEvent* event);

    private slots:
        void openFiles();
        void helpDialog();
        void aboutDialog();
        void setBackground(const QString& name);
        void chooseBackground();
        void setColorChannels(QStringList channels);

    private:
        PointView* m_pointView;
        QMenu* m_colorMenu;
        QActionGroup* m_colorMenuGroup;
        QSignalMapper* m_colorMenuMapper;
};



#endif // AQSIS_PTVIEW_H_INCLUDED

// vi: set et:
