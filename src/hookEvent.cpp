// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "hookEvent.h"


hookEvent::hookEvent(QObject* parent)
    : QObject(parent),
    m_shortCut(),
    m_hookSpec()
{
}

/// Set shortcut
void hookEvent::setHookEvent(QByteArray hookSpec)
{
    int numberShortCuts = m_shortCut.count();

    /// Go through all shortcuts, see if one exists, if not create it
    QKeySequence requested = (QKeySequence::fromString(QString::fromUtf8(hookSpec), QKeySequence::PortableText));
    bool taken = false;
    for(int i=0; i<numberShortCuts; i++)
    {
        if(getHookSpec(i)==requested)
        {
            taken = true;
            latestHookIndex = i;
        }
    }

    if(!taken)
    {
        m_hookSpec.append(QKeySequence::fromString(QString::fromUtf8(hookSpec), QKeySequence::PortableText));
        m_shortCut.append(new QShortcut(m_hookSpec.at(numberShortCuts), qobject_cast<QWidget*>(this->parent())));
        latestHookIndex = numberShortCuts;
    }
}
