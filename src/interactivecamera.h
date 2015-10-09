// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

/// \author Chris Foster [chris42f [at] gmail (dot) com]

#ifndef AQSIS_INTERACTIVE_CAMERA_H_INCLUDED
#define AQSIS_INTERACTIVE_CAMERA_H_INCLUDED

#ifdef _MSC_VER
#   ifndef _USE_MATH_DEFINES
#       define _USE_MATH_DEFINES
#   endif
#endif
#include <cmath>

#include <QQuaternion>
#include <QVector3D>
#include <QMatrix4x4>
#include <QRect>

#include <OpenEXR/ImathVec.h>
#include <OpenEXR/ImathMatrix.h>

/// Camera controller for mouse-based scene navigation
///
/// The camera model used here is for inspecting objects, so we have a location
/// of interest - the camera "center" - which the eye always looks at and
/// around which the eye can be rotated with the mouse.  There are two possible
/// rotation models supported here:
///
/// 1) The virtual trackball model  - this does not impose any particular "up
///    vector" on the user.
/// 2) The turntable model, which is potentially more intuitive when the data
///    has a natural vertical direction.
///
class InteractiveCamera : public QObject
{
    Q_OBJECT

    public:
        /// Construct camera; if reverseHandedness is true, the viewing
        /// transformation will invert the z-axis.  If used with OpenGL (which
        /// is right handed by default) this gives us a left handed coordinate
        /// system.
        explicit InteractiveCamera(bool reverseHandedness = false,
                                   bool trackballInteraction = true)
            : m_reverseHandedness(reverseHandedness),
            m_trackballInteraction(trackballInteraction),
            m_rot(),
            m_center(0,0,0),
            m_dist(5),
            m_fieldOfView(60),
            m_clipNear(0.1),
            m_clipFar(1000)
        { }

        /// Get the projection from camera to screen coordinates
        Imath::M44d projectionMatrix() const
        {
            QMatrix4x4 m;
            qreal aspect = qreal(m_viewport.width())/m_viewport.height();
            m.perspective(m_fieldOfView, aspect, m_clipNear, m_clipFar);
            return qt2exr(m);
        }

        /// Get view transformation from world to camera coordinates
        Imath::M44d viewMatrix() const
        {
            QMatrix4x4 m;
            m.translate(0, 0, -m_dist);
            m.rotate(m_rot);
            if(m_reverseHandedness)
                m.scale(1,1,-1);
            return qt2exr(m).translate(-m_center);
        }

        /// Get transformation from screen coords to viewport coords
        ///
        /// The viewport coordinates are in pixels, with 0,0 at the top left
        /// and width,height at the bottom right.
        Imath::M44d viewportMatrix() const
        {
            QMatrix4x4 m;
            m.translate(m_viewport.x(), m_viewport.y(), 0);
            m.scale(0.5*m_viewport.width(), -0.5*m_viewport.height(), 1);
            m.translate(1, -1, 0);
            return qt2exr(m);
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
        Imath::V3d center() const   { return m_center; }
        /// Get position of camera
        Imath::V3d position() const { return Imath::V3d(0,0,0)*viewMatrix().inverse(); }
        /// Get distance from eye to center
        qreal eyeToCenterDistance() const { return m_dist; }
        /// Get the rotation about the center
        QQuaternion rotation() const { return m_rot; }
        /// Get the interaction mode
        bool trackballInteraction() const { return m_trackballInteraction; }

        /// Grab and move a point in the 3D space with the mouse.
        ///
        /// p is the point to move in world coordinates.  mouseMovement is a
        /// vector moved by the mouse inside the 2D viewport.  If zooming is
        /// true, the point will be moved along the viewing direction rather
        /// than perpendicular to it.
        Imath::V3d mouseMovePoint(Imath::V3d p, QPoint mouseMovement,
                                  bool zooming) const
        {
            qreal dx = 2*qreal(mouseMovement.x())/m_viewport.width();
            qreal dy = 2*qreal(-mouseMovement.y())/m_viewport.height();
            if(zooming)
            {
                Imath::M44d view = viewMatrix();
                return (p*view*std::exp(dy)) * view.inverse();
            }
            else
            {
                Imath::M44d proj = viewMatrix()*projectionMatrix();
                return (p*proj + Imath::V3d(dx, dy, 0)) * proj.inverse();
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
        void setCenter(Imath::V3d center)
        {
            m_center = center;
            emit viewChanged();
        }
        void setEyeToCenterDistance(qreal dist)
        {
            m_dist = dist;
            emit viewChanged();
        }
        void setRotation(QQuaternion rotation)
        {
            m_rot = rotation;
            emit viewChanged();
        }
        void setTrackballInteraction(bool trackballInteraction)
        {
            m_trackballInteraction = trackballInteraction;
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
                if(m_trackballInteraction)
                    m_rot = trackballRotation(prevPos, currPos) * m_rot;
                else
                {
                    // TODO: Not sure this is entirely consistent if the user
                    // switches between trackball and turntable modes...
                    m_rot = turntableRotation(prevPos, currPos, m_rot);
                }
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
        /// Perform "turntable" style rotation on current orientation
        ///
        /// currPos is the new position of the mouse pointer; prevPos is the
        /// previous position.  initialRot is the current camera orientation,
        /// which will be modified by the mouse movement and returned.
        QQuaternion turntableRotation(QPoint prevPos, QPoint currPos,
                                      QQuaternion initialRot) const
        {
            qreal dx = 4*qreal(currPos.x() - prevPos.x())/m_viewport.width();
            qreal dy = 4*qreal(currPos.y() - prevPos.y())/m_viewport.height();
            QQuaternion r1 = QQuaternion::fromAxisAndAngle(QVector3D(1,0,0), 180/M_PI*dy);
            QQuaternion r2 = QQuaternion::fromAxisAndAngle(QVector3D(0,0,1), 180/M_PI*dx);
            return r1 * initialRot * r2;
        }

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


        //----------------------------------------------------------------------
        // Qt->Ilmbase vector/matrix conversions.
        // TODO: Refactor everything to use a consistent set of matrix/vector
        // classes (replace Ilmbase with Eigen?)
        template<typename T>
        static inline QVector3D exr2qt(const Imath::Vec3<T>& v)
        {
            return QVector3D(v.x, v.y, v.z);
        }

        static inline Imath::V3d qt2exr(const QVector3D& v)
        {
            return Imath::V3d(v.x(), v.y(), v.z());
        }

        static inline Imath::M44d qt2exr(const QMatrix4x4& m)
        {
            Imath::M44d mOut;
            for(int j = 0; j < 4; ++j)
            for(int i = 0; i < 4; ++i)
                mOut[j][i] = m.constData()[4*j + i];
            return mOut;
        }

        bool m_reverseHandedness; ///< Reverse the handedness of the coordinate system
        bool m_trackballInteraction; ///< True for trackball style, false for turntable

        // World coordinates
        QQuaternion m_rot;    ///< camera rotation about center
        Imath::V3d m_center;   ///< center of view for camera
        qreal m_dist;         ///< distance from center of view
        // Projection variables
        qreal m_fieldOfView;  ///< field of view in degrees
        qreal m_clipNear;     ///< near clipping plane
        qreal m_clipFar;      ///< far clipping plane
        QRect m_viewport;     ///< rectangle we'll drag inside
};


#endif // AQSIS_INTERACTIVE_CAMERA_H_INCLUDED
