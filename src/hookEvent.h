// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef DISPLAZ_HOOKEVENT_H_INCLUDED
#define DISPLAZ_HOOKEVENT_H_INCLUDED

#include <QByteArray>
#include <QKeySequence>
#include <QShortcut>

/// Class for handling hook events. Implemented via QShortCut,
/// for more generic event handling define custom events here.
///
class hookEvent : public QObject
{
    Q_OBJECT
    public:
        hookEvent(QObject* parent = NULL);

        QKeySequence getHookSpec(int index) const { return m_hookSpec.at(index); }
        void setHookEvent(QByteArray hookSpec);
        int getHookId() const { return latestHookIndex; }
        QVector<QShortcut*> m_shortCut;

    private:
        QList<QKeySequence> m_hookSpec;
        int latestHookIndex;
};

#endif // DISPLAZ_HOOKEVENT_H_INCLUDED
