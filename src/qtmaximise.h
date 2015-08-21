// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef QT_MAXIMISE_H_INCLUDED
#define QT_MAXIMISE_H_INCLUDED

#include <QAction>
#include <QMainWindow>

class QActionMaximise : public QAction
{
    Q_OBJECT

    public:
        QActionMaximise(QMainWindow* mainWindow = 0) : QAction(tr("Maximise"),mainWindow), m_mainWindow(mainWindow)
        {
            setShortcut(Qt::Key_F11);
            connect(this, SIGNAL(triggered(bool)), this, SLOT(handleTriggered(bool)));
        }

    public slots:
        void handleTriggered(bool checked)
        {
            const bool maximised = m_mainWindow->windowState()&Qt::WindowMaximized;
            if (maximised)
                m_mainWindow->showNormal();
            else
                m_mainWindow->showMaximized();
        }

    private:
        QMainWindow* m_mainWindow;
};

#endif // QT_MAXIMISE_H_INCLUDED
