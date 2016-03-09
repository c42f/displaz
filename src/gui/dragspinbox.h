// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef DRAGSPINBOX_H_INCLUDED
#define DRAGSPINBOX_H_INCLUDED

#include <math.h>

#include <QDoubleSpinBox>
#include <QMouseEvent>
#include <QApplication>
#include <QDesktopWidget>

/// Hacked version of QDoubleSpinBox which allows vertical mouse dragging to
/// change the current value.
///
/// This is extremely handy and intuitive for some purposes, since the value in
/// the spin box mirrors the magnitude of motion of the mouse.
///
/// Scaling of the value during the drag can be exponential or linear,
/// depending on the value in the constructor.  Exponential scaling is usually
/// what you want for positive values with a large range.
class DragSpinBox : public QDoubleSpinBox
{
    Q_OBJECT

    public:
        DragSpinBox(bool exponentialScaling = true, double speed = 1,
                    QWidget* parent = 0)
            : QDoubleSpinBox(parent),
            m_speed(speed),
            m_exponentialScaling(exponentialScaling)
        {
            setMouseTracking(false);
            setCursor(Qt::SplitVCursor);
        }

    protected:
        void mousePressEvent(QMouseEvent* event)
        {
            m_pressPos = event->pos();
            m_prevPos = event->pos();
            event->accept();
            setCursor(Qt::BlankCursor);
        }

        void mouseReleaseEvent(QMouseEvent* event)
        {
            QCursor::setPos(mapToGlobal(m_pressPos));
            setCursor(Qt::SplitVCursor);
        }

        void mouseMoveEvent(QMouseEvent* event)
        {
            event->accept();
            int dy = -(event->pos().y() - m_prevPos.y());
            m_prevPos = event->pos();
            QRect geom = QApplication::desktop()->screenGeometry(this);
            // Calling setPos() creates a further mouse event asynchronously;
            // this is a hack to suppress such unwanted events:
            if (abs(dy) > geom.height()/2)
                return;
            double val = value();
            if (m_exponentialScaling)
                val *= exp(0.01*m_speed*dy); // scale proportional to value
            else
                val += 0.1*m_speed*dy;  // linear scaling
            setValue(val);
            // Wrap when mouse goes off top or bottom of screen
            QPoint gpos = mapToGlobal(event->pos());
            if (gpos.y() == geom.top())
                QCursor::setPos(QPoint(gpos.x(), geom.bottom()-1));
            if (gpos.y() == geom.bottom())
                QCursor::setPos(QPoint(gpos.x(), geom.top()+1));
        }

    private:
        QPoint m_pressPos;
        QPoint m_prevPos;
        double m_speed;
        bool m_exponentialScaling;
};


#endif // DRAGSPINBOX_H_INCLUDED
