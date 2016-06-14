// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef DISPLAZ_HOOKMANAGER_H_INCLUDED
#define DISPLAZ_HOOKMANAGER_H_INCLUDED

#include <QByteArray>
#include <QKeySequence>
#include <QShortcut>

class HookFormatter;

/// Class for registering custom hook events. Implemented via QShortCut,
/// i.e. for QKeySequences only.
class HookManager : public QObject
{
    Q_OBJECT
    public:
        HookManager(QObject* parent = NULL);

        /// Connect events defined by `eventSpec` to the HookFormatter hookActivated() function.
        void connectHook(QByteArray eventSpec, HookFormatter* formatter);

    public slots:
        void deactivateEvent(int eventId);

    private:
        QVector<QShortcut*> m_events;
        QList<QKeySequence> m_eventSpecs;
        QVector<int> m_hooksSignedUp;
};

#endif // DISPLAZ_HOOKMANAGER_H_INCLUDED
