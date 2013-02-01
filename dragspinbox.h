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

#ifndef DRAGSPINBOX_H_INCLUDED
#define DRAGSPINBOX_H_INCLUDED

#include <math.h>

#include <QtGui/QDoubleSpinBox>
#include <QtGui/QMouseEvent>
#include <QtGui/QApplication>
#include <QtGui/QDesktopWidget>

/// Hacked version of QDoubleSpinBox which allows vertical mouse dragging to
/// change the current value.
///
/// This is extremely handy and intuitive for some purposes, since the value in
/// the spin box mirrors the magnitude of motion of the mouse.
class DragSpinBox : public QDoubleSpinBox
{
    Q_OBJECT

    public:
        DragSpinBox(QWidget* parent = 0)
            : QDoubleSpinBox(parent)
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
            int y = event->pos().y();
            int dy = -(y - m_prevPos.y());
            m_prevPos = event->pos();
            QRect geom = QApplication::desktop()->screenGeometry(this);
            // Calling setPos() creates a further mouse event asynchronously;
            // this is a hack to suppress such unwanted events:
            if (abs(dy) > geom.height()/2)
                return;
            double val = value();
            val *= exp(dy/100.0); // exponential scaling
            //val += dy;  // linear scaling
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
};


#endif // DRAGSPINBOX_H_INCLUDED
