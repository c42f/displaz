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

#include <QTimer>
#include <QTime>

#include <QtGui/QQuaternion>
#include <QtGui/QVector3D>
#include <QtGui/QMatrix4x4>
#include <QtCore/QRect>

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

        /// Adjusts rotation so that rotated dir aligns with (+x|+y|+z|-x|-y|-z) axis
        static void snapRotationToAxis(const QVector3D& dir, QQuaternion& rot);

        /// Construct camera; if reverseHandedness is true, the viewing
        /// transformation will invert the z-axis.  If used with OpenGL (which
        /// is right handed by default) this gives us a left handed coordinate
        /// system.
        InteractiveCamera(bool reverseHandedness = false,
                          bool trackballInteraction = true);

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
        /// Get distance from eye to center
        qreal eyeToCenterDistance() const { return m_dist; }
        /// Get the rotation about the center
        QQuaternion rotation() const { return m_rot; }
        /// Get whether animation is still proceeding
        bool isAnimating() const;
        /// Get the projection from camera to screen coordinates
        Imath::M44d projectionMatrix() const;
        /// Get view transformation from world to camera coordinates
        Imath::M44d viewMatrix() const;
        /// Get transformation from screen coords to viewport coords
        /// The viewport coordinates are in pixels, with 0,0 at the top left
        /// and width,height at the bottom right.
        Imath::M44d viewportMatrix() const;
        /// Get position of camera
        Imath::V3d position() const;

        /// Grab and move a point in the 3D space with the mouse.
        /// p is the point to move in world coordinates.  mouseMovement is a
        /// vector moved by the mouse inside the 2D viewport.  If zooming is
        /// true, the point will be moved along the viewing direction rather
        /// than perpendicular to it.
        Imath::V3d mouseMovePoint(Imath::V3d p, QPoint mouseMovement,
                                  bool zooming) const;

        void setViewport(QRect rect);
        void setClipNear(qreal clipNear);
        void setClipFar(qreal clipFar);
        void setFieldOfView(qreal fov);
        void setCenter(Imath::V3d center);
        void setEyeToCenterDistance(qreal dist);
        void setRotation(QQuaternion rotation);

        void animateTo(const QQuaternion& newRotation,
                       const Imath::V3d& newCenter,
                       const qreal newEyeToCenterDistance);

        /// Move the camera using a drag of the mouse.
        /// The previous and current positions of the mouse during the move are
        /// given by prevPos and currPos.  By default this rotates the camera
        /// around the center, but if zoom is true, the camera position is
        /// zoomed in toward the center instead.
        void mouseDrag(QPoint prevPos, QPoint currPos, bool zoom = false);

    public slots:
        void toggleTrackballMode();
        void toggleAnimateViewTransformMode();


    signals:
        /// The projection matrix has changed
        void projectionChanged();
        /// The view matrix has changed
        void viewChanged();

    private slots:
        void nextFrame();

    private:
        /// Perform "turntable" style rotation on current orientation
        /// currPos is the new position of the mouse pointer; prevPos is the
        /// previous position.  initialRot is the current camera orientation,
        /// which will be modified by the mouse movement and returned.
        QQuaternion turntableRotation(QPoint prevPos, QPoint currPos,
                                      QQuaternion initialRot) const;

        /// Get rotation of trackball.
        /// currPos is the new position of the mouse pointer; prevPos is the
        /// previous position.  For the parameters chosen here, moving the
        /// mouse around any closed curve will give a composite rotation of the
        /// identity.  This is rather important for the predictability of the
        /// user interface.
        QQuaternion trackballRotation(QPoint prevPos, QPoint currPos) const;

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
        QVector3D trackballVector(QPoint pos, qreal r) const;

        bool m_reverseHandedness;    ///< Reverse the handedness of the coordinate system
        bool m_trackballInteraction; ///< True for trackball style, false for turntable

        // World coordinates
        QQuaternion m_rot;    ///< camera rotation about center
        Imath::V3d m_center;  ///< center of view for camera
        qreal m_dist;         ///< distance from center of view
        // Projection variables
        qreal m_fieldOfView;  ///< field of view in degrees
        qreal m_clipNear;     ///< near clipping plane
        qreal m_clipFar;      ///< far clipping plane
        QRect m_viewport;     ///< rectangle we'll drag inside
        /// Animated view transform
        QTimer*           m_animateTimer;
        QTime             m_animateTime;
        int               m_animateDuration;  // msec
        QQuaternion m_rotStart,    m_rotEnd;     ///< start and end rotation position for animation
        Imath::V3d  m_centerStart, m_centerEnd; ///< star and end camera centers
        qreal       m_distStart,   m_distEnd;   ///< start and end zoom distance
};


#endif // AQSIS_INTERACTIVE_CAMERA_H_INCLUDED
