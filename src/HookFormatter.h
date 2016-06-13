// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef DISPLAZ_HOOKFORMATTER_H_INCLUDED
#define DISPLAZ_HOOKFORMATTER_H_INCLUDED

#include <QObject>
#include <QByteArray>

class PointViewerMainWindow;

/// Class for formatting hook event messages

class HookFormatter : public QObject
{
    Q_OBJECT
    public:
        HookFormatter(PointViewerMainWindow* getPayload, QByteArray hookSpec,
                          QByteArray hookPayload, QObject* parent = NULL);

        void setEventId(int eventId) { m_eventId = eventId; }

    public slots:
        void hookActivated(void);

    signals:
        /// Triggered on event of custom shortcut
        void sendIpcMessage(QByteArray message);

        /// Triggered on disconnect of parent channel
        void hookRemoved(int eventId);

    private slots:
       void channelDisconnected(void) { emit hookRemoved(m_eventId); }

    private:
        QByteArray m_hookSpec;
        QByteArray m_hookPayload;
        PointViewerMainWindow* m_getPayload;
        int m_eventId;
};

#endif // DISPLAZ_HOOKFORMATTER_H_INCLUDED
