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

/// \author Chris Foster [chris42f [at] gmail (dot) com]

#ifndef AQSIS_INTERACTIVE_CAMERA_H_INCLUDED
#define AQSIS_INTERACTIVE_CAMERA_H_INCLUDED

#include <cmath>

#include <QtGui/QQuaternion>
#include <QtGui/QVector3D>
#include <QtGui/QMatrix4x4>
#include <QtCore/QRect>


/// Camera controller for mouse-based scene navigation
///
/// The camera model used here is for inspecting objects, so we have a location
/// of interest - the camera "center" - which the eye always looks at and
/// around which the eye can be rotated with the mouse.  The rotation model
/// used is a virtual trackball so as not to impose any particular up vector on
/// the user.
class InteractiveCamera : public QObject
{
    Q_OBJECT

    public:
        /// Construct camera; if reverseHandedness is true, the viewing
        /// transformation will invert the z-axis.  If used with OpenGL (which
        /// is right handed by default) this gives us a left handed coordinate
        /// system.
        explicit InteractiveCamera(bool reverseHandedness = false)
            : m_reverseHandedness(reverseHandedness),
            m_rot(),
            m_center(0,0,0),
            m_dist(5),
            m_fieldOfView(60),
            m_clipNear(0.1),
            m_clipFar(1000)
        { }

        /// Get the projection from camera to screen coordinates
        QMatrix4x4 projectionMatrix() const
        {
            QMatrix4x4 m;
            qreal aspect = qreal(m_viewport.width())/m_viewport.height();
            m.perspective(m_fieldOfView, aspect, m_clipNear, m_clipFar);
            return m;
        }

        /// Get view transformation from world to camera coordinates
        QMatrix4x4 viewMatrix() const
        {
            QMatrix4x4 m;
            m.translate(0, 0, -m_dist);
            m.rotate(m_rot);
            if(m_reverseHandedness)
                m.scale(1,1,-1);
            m.translate(-m_center);
            return m;
        }

        /// Get the 2D region associated with the camera
        QRect viewport() const     { return m_viewport; }
        /// Get depth of near clipping plane
        qreal clipNear() const     { return m_clipNear; }
        /// Get depth of far clipping plane
        qreal clipFar() const      { return m_clipFar; }
        /// Get field of view
        qreal fieldOfView() const  { return m_fieldOfView; }
        /// Get center around which the camera will pivot
        QVector3D center() const   { return m_center; }
        /// Get distance from eye to center
        qreal eyeToCenterDistance() const { return m_dist; }

        /// Grab and move a point in the 3D space with the mouse.
        ///
        /// p is the point to move in world coordinates.  mouseMovement is a
        /// vector moved by the mouse inside the 2D viewport.  If zooming is
        /// true, the point will be moved along the viewing direction rather
        /// than perpendicular to it.
        QVector3D mouseMovePoint(QVector3D p, QPoint mouseMovement,
                                 bool zooming) const
        {
            qreal dx = 2*qreal(mouseMovement.x())/m_viewport.width();
            qreal dy = 2*qreal(-mouseMovement.y())/m_viewport.height();
            if(zooming)
            {
                QMatrix4x4 view = viewMatrix();
                return view.inverted().map(view.map(p)*std::exp(dy));
            }
            else
            {
                QMatrix4x4 proj = projectionMatrix()*viewMatrix();
                return proj.inverted().map(proj.map(p) + QVector3D(dx, dy, 0));
            }
        }

    public slots:
        void setViewport(QRect rect)
        {
            m_viewport = rect;
            emit viewChanged();
        }
        void setClipNear(qreal clipNear)
        {
            m_clipNear = clipNear;
            emit projectionChanged();
        }
        void setClipFar(qreal clipFar)
        {
            m_clipFar = clipFar;
            emit projectionChanged();
        }
        void setFieldOfView(qreal fov)
        {
            m_fieldOfView = fov;
            emit projectionChanged();
        }
        void setCenter(QVector3D center)
        {
            m_center = center;
            emit viewChanged();
        }
        void setEyeToCenterDistance(qreal dist)
        {
            m_dist = dist;
            emit viewChanged();
        }

        /// Move the camera using a drag of the mouse.
        ///
        /// The previous and current positions of the mouse during the move are
        /// given by prevPos and currPos.  By default this rotates the camera
        /// around the center, but if zoom is true, the camera position is
        /// zoomed in toward the center instead.
        void mouseDrag(QPoint prevPos, QPoint currPos, bool zoom = false)
        {
            if(zoom)
            {
                // exponential zooming gives scale-independent sensitivity
                qreal dy = qreal(currPos.y() - prevPos.y())/m_viewport.height();
                const qreal zoomSpeed = 3.0f;
                m_dist *= std::exp(zoomSpeed*dy);
            }
            else
            {
                // Concatenate rotation, trackball-style.
                m_rot = trackballRotation(prevPos, currPos) * m_rot;
                m_rot.normalize();
            }
            emit viewChanged();
        }

    signals:
        /// The projection matrix has changed
        void projectionChanged();
        /// The view matrix has changed
        void viewChanged();

    private:
        /// Get rotation of trackball.
        ///
        /// currPos is the new position of the mouse pointer; prevPos is the
        /// previous position.  For the parameters chosen here, moving the
        /// mouse around any closed curve will give a composite rotation of the
        /// identity.  This is rather important for the predictability of the
        /// user interface.
        QQuaternion trackballRotation(QPoint prevPos, QPoint currPos) const
        {
            // Compute the new and previous positions of the cursor on a 3D
            // virtual trackball.  Form a rotation around the axis which would
            // take the previous position to the new position.
            const qreal trackballRadius = 1.1; // as in blender
            QVector3D p1 = trackballVector(prevPos, trackballRadius);
            QVector3D p2 = trackballVector(currPos, trackballRadius);
            QVector3D axis = QVector3D::crossProduct(p1, p2);
            // The rotation angle between p1 and p2 in radians is
            //
            // std::asin(axis.length()/(p1.length()*p2.length()));
            //
            // However, it's preferable to use two times this angle for the
            // rotation instead: It's a remarkable fact that the total rotation
            // after moving the mouse through any closed path is then the
            // identity, which means the model returns exactly to its previous
            // orientation when you return the mouse to the starting position.
            qreal angle = 2*std::asin(axis.length()/(p1.length()*p2.length()));
            return QQuaternion::fromAxisAndAngle(axis, 180/M_PI*angle);
        }

        /// Get position on surface of a virtual trackball
        ///
        /// The classic trackball camera control projects a position on the
        /// screen orthogonally onto a sphere to compute a 3D cursor position.
        /// The sphere is centred at the middle of the screen, with some
        /// diameter chosen to taste but roughly the width of the screen.
        ///
        /// This projection doesn't make sense at all points in the plane, so
        /// we join a cone smoothly to the sphere at distance r/sqrt(2) so that
        /// all the points at larger radii are projected onto the cone instead.
        ///
        /// Historical note: The trackball code for blender's default camera
        /// seems to have been inspired by GLUT's trackball.c by Gavin Bell
        /// (aka Gavin Andresen).  Those codes use a hyperboloid rather than a
        /// cone, but I've used a cone here to improve mouse sensitivity near
        /// the edge of the view port without resorting to the no-asin() hack
        /// used by blender. - CJF
        QVector3D trackballVector(QPoint pos, qreal r) const
        {
            // Map x & y mouse locations to the interval [-1,1]
            qreal x =  2.0*(pos.x() - m_viewport.center().x())/m_viewport.width();
            qreal y = -2.0*(pos.y() - m_viewport.center().y())/m_viewport.height();
            qreal d = sqrt(x*x + y*y);
            // get projected z coordinate -      sphere : cone
            qreal z = (d < r/M_SQRT2) ? sqrt(r*r - d*d) : r*M_SQRT2 - d;
            return QVector3D(x,y,z);
        }

        bool m_reverseHandedness; ///< Reverse the handedness of the coordinate system

        // World coordinates
        QQuaternion m_rot;    ///< camera rotation about center
        QVector3D m_center;   ///< center of view for camera
        qreal m_dist;         ///< distance from center of view
        // Projection variables
        qreal m_fieldOfView;  ///< field of view in degrees
        qreal m_clipNear;     ///< near clipping plane
        qreal m_clipFar;      ///< far clipping plane
        QRect m_viewport;     ///< rectangle we'll drag inside
};


#endif // AQSIS_INTERACTIVE_CAMERA_H_INCLUDED
