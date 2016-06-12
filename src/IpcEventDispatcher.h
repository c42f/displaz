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
        IpcEventDispatcher(QObject* parent = NULL, PointViewerMainWindow* getPayload = NULL, QByteArray = 0, QByteArray = 0);

    public slots:
        void hookActivated(void);

    signals:
        /// Triggered on event of custom shortcut
        void sendIpcMessage(QByteArray message);

    private:
        QKeySequence m_hookSpec;
        QByteArray m_hookPayload;
        PointViewerMainWindow* m_getPayload;
};

#endif // DISPLAZ_IPCED_H_INCLUDED
