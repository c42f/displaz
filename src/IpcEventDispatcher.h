// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef DISPLAZ_IPCED_H_INCLUDED
#define DISPLAZ_IPCED_H_INCLUDED

#include <QByteArray>
#include <QKeySequence>
#include <QShortcut>

class PointViewerMainWindow;

/// Class for dispatching hook events and payloads to given IpcChannel
///
class IpcEventDispatcher : public QObject
{
    Q_OBJECT
    public:
        IpcEventDispatcher(QObject* parent = NULL, PointViewerMainWindow* getPayload = NULL,
                           QByteArray = 0, QByteArray = 0, int = 0);

    public slots:
        void hookActivated(void);
        void removedHook(void) { emit deletedShortCut(m_whichShortCut); }

    signals:
        /// Triggered on event of custom shortcut
        void sendIpcMessage(QByteArray message);

        /// Triggered on disconnect of parent channel
        void deletedShortCut(int whichShortCut);

    private:
        QByteArray m_hookSpec;
        QByteArray m_hookPayload;
        PointViewerMainWindow* m_getPayload;
        int m_whichShortCut;
};

#endif // DISPLAZ_IPCED_H_INCLUDED
