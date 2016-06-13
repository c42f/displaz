// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef DISPLAZ_HOOKEVENT_H_INCLUDED
#define DISPLAZ_HOOKEVENT_H_INCLUDED

#include <QByteArray>
#include <QKeySequence>
#include <QShortcut>

/// Class for registering custom hook events. Implemented via QShortCut,
/// i.e. for QKeySequences only.
class hookEvent : public QObject
{
    Q_OBJECT
    public:
        hookEvent(QObject* parent = NULL);

        void setHookEvent(QByteArray hookSpec);
        int getHookId() const { return latestHookIndex; }
        QVector<QShortcut*> m_shortCut;

    public slots:
        void removeShortCut(int whichShortCut);

    private:
        QList<QKeySequence> m_hookSpec;
        QVector<int> m_hooksSignedUp;
        int latestHookIndex;
};

#endif // DISPLAZ_HOOKEVENT_H_INCLUDED
