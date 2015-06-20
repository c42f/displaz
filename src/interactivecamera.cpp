// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "interactivecamera.h"

#include <QtCore/QTimer>
#include <QtCore/QTime>

//----------------------------------------------------------------------
// Qt->Ilmbase vector/matrix conversions.
// TODO: Refactor everything to use a consistent set of matrix/vector
// classes (replace Ilmbase with Eigen?)
template<typename T>
inline QVector3D exr2qt(const Imath::Vec3<T>& v)
{
    return QVector3D(v.x, v.y, v.z);
}

inline Imath::V3d qt2exr(const QVector3D& v)
{
    return Imath::V3d(v.x(), v.y(), v.z());
}

inline Imath::M44d qt2exr(const QMatrix4x4& m)
{
    Imath::M44d mOut;
    for(int j = 0; j < 4; ++j)
        for(int i = 0; i < 4; ++i)
            mOut[j][i] = m.constData()[4*j + i];
    return mOut;
}

void InteractiveCamera::snapRotationToAxis(const QVector3D& dir, QQuaternion& rot)
{
    QMatrix4x4 m;

    m.rotate(rot);
    QVector3D e = dir * m;
    QVector3D newE;
    if(fabs(e.x()) > std::max(fabs(e.y()), fabs(e.z())))
        newE = QVector3D(Imath::sign(e.x()), 0, 0);
    else if(fabs(e.y()) > fabs(e.z()))
        newE = QVector3D(0, Imath::sign(e.y()), 0);
    else
        newE = QVector3D(0, 0, Imath::sign(e.z()));
    e = QVector3D::crossProduct(newE, e);
    const double a = asin(e.length());
    e.normalize();
    rot *= QQuaternion::fromAxisAndAngle(e, a*180/M_PI); // this function wants angle in deg!
}


InteractiveCamera::InteractiveCamera(bool reverseHandedness,
                                     bool trackballInteraction)
    : m_reverseHandedness(reverseHandedness),
      m_trackballInteraction(trackballInteraction),
      m_rot(),
      m_center(0,0,0),
      m_dist(5),
      m_fieldOfView(60),
      m_clipNear(0.1),
      m_clipFar(1000),
      m_animateDuration(250)
{
    m_animateTimer = new QTimer(this);
    m_animateTimer->setSingleShot(false);
    connect(m_animateTimer, SIGNAL(timeout()), this, SLOT(nextFrame()));
}


bool InteractiveCamera::isAnimating() const
{
    return m_animateTimer->isActive();
}


Imath::M44d InteractiveCamera::projectionMatrix() const
{
    QMatrix4x4 m;
    qreal aspect = qreal(m_viewport.width())/m_viewport.height();
    m.perspective(m_fieldOfView, aspect, m_clipNear, m_clipFar);
    return qt2exr(m);
}


Imath::M44d InteractiveCamera::viewMatrix() const
{
    QMatrix4x4 m;
    m.translate(0, 0, -m_dist);
    m.rotate(m_rot);
    if(m_reverseHandedness)
        m.scale(1,1,-1);
    return qt2exr(m).translate(-m_center);
}


Imath::M44d InteractiveCamera::viewportMatrix() const
{
    QMatrix4x4 m;
    m.translate(m_viewport.x(), m_viewport.y(), 0);
    m.scale(0.5*m_viewport.width(), -0.5*m_viewport.height(), 1);
    m.translate(1, -1, 0);
    return qt2exr(m);
}


Imath::V3d InteractiveCamera::position() const
{
    return Imath::V3d(0,0,0)*viewMatrix().inverse();
}


Imath::V3d InteractiveCamera::mouseMovePoint(Imath::V3d p, QPoint mouseMovement,
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


void InteractiveCamera::setViewport(QRect rect)
{
    m_viewport = rect;
    emit viewChanged();
}


void InteractiveCamera::setClipNear(qreal clipNear)
{
    m_clipNear = clipNear;
    emit projectionChanged();
}


void InteractiveCamera::setClipFar(qreal clipFar)
{
    m_clipFar = clipFar;
    emit projectionChanged();
}


void InteractiveCamera::setFieldOfView(qreal fov)
{
    m_fieldOfView = fov;
    emit projectionChanged();
}


void InteractiveCamera::setCenter(Imath::V3d center)
{
    m_center = center;
    emit viewChanged();
}


void InteractiveCamera::setEyeToCenterDistance(qreal dist)
{
    m_dist = dist;
    emit viewChanged();
}


void InteractiveCamera::setRotation(QQuaternion rotation)
{
    m_rot = rotation.normalized();
    emit viewChanged();
}


void InteractiveCamera::animateTo(const QQuaternion& newRotation,
                                  const Imath::V3d& newCenter,
                                  const qreal newEyeToCenterDistance)
{
    // check that a transform is actually required
    if(   (m_center - newCenter).length() < 1e-3
       && (m_rot  - newRotation).length() < 1e-3
       && fabs(m_dist - newEyeToCenterDistance) < 1e-3)
        return;
    // set start camera postion to current camera
    m_rotStart    = m_rot;
    m_centerStart = m_center;
    m_distStart   = m_dist;
    // set end camera postion to new values
    m_rotEnd      = newRotation;
    m_centerEnd   = newCenter;
    m_distEnd     = newEyeToCenterDistance;
    // start transform
    m_animateTime.restart();
    m_animateTimer->start(20);
}


void InteractiveCamera::mouseDrag(QPoint prevPos, QPoint currPos, bool zoom)
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


void InteractiveCamera::toggleTrackballMode()
{
    m_trackballInteraction = !m_trackballInteraction;
}


void InteractiveCamera::toggleAnimateViewTransformMode()
{
    if(m_animateDuration == 0)
        m_animateDuration = 250;
    else
        m_animateDuration = 0;
}


void InteractiveCamera::nextFrame()
{
    if(m_animateDuration > 0)
    {
        double xsi = m_animateTime.elapsed()/double(m_animateDuration);
        xsi = std::min(xsi,1.0);
        xsi = xsi*xsi*(3 - 2*xsi);

        m_rot = QQuaternion::slerp(m_rotStart, m_rotEnd, xsi);
        m_center = (1 - xsi)*m_centerStart + xsi*m_centerEnd;
        m_dist = pow(m_distStart, 1 - xsi)*pow(m_distEnd, xsi);

        if (xsi>=1.0)
            m_animateTimer->stop();
    }
    else
    {
        m_rot    = m_rotEnd;
        m_center = m_centerEnd;
        m_dist   = m_distEnd;
        m_animateTimer->stop();
    }

    emit viewChanged();
}

QQuaternion InteractiveCamera::turntableRotation(QPoint prevPos, QPoint currPos,
                              QQuaternion initialRot) const
{
    qreal dx = 4*qreal(currPos.x() - prevPos.x())/m_viewport.width();
    qreal dy = 4*qreal(currPos.y() - prevPos.y())/m_viewport.height();
    QQuaternion r1 = QQuaternion::fromAxisAndAngle(QVector3D(1,0,0), 180/M_PI*dy);
    QQuaternion r2 = QQuaternion::fromAxisAndAngle(QVector3D(0,0,1), 180/M_PI*dx);
    return r1 * initialRot * r2;
}


QQuaternion InteractiveCamera::trackballRotation(QPoint prevPos, QPoint currPos) const
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


QVector3D InteractiveCamera::trackballVector(QPoint pos, qreal r) const
{
    // Map x & y mouse locations to the interval [-1,1]
    qreal x =  2.0*(pos.x() - m_viewport.center().x())/m_viewport.width();
    qreal y = -2.0*(pos.y() - m_viewport.center().y())/m_viewport.height();
    qreal d = sqrt(x*x + y*y);
    // get projected z coordinate -      sphere : cone
    qreal z = (d < r/M_SQRT2) ? sqrt(r*r - d*d) : r*M_SQRT2 - d;
    return QVector3D(x,y,z);
}



