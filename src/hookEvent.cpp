// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include <QRegExp>

#include "hookEvent.h"


hookEvent::hookEvent(QObject* parent)
    : QObject(parent),
    m_shortCut(),
    m_hookSpec(),
    m_hooksSignedUp()
{
}


/// Set shortcut
void hookEvent::setHookEvent(QByteArray hookSpec)
{
    /// Remove keyword before key sequence, indicated by colon. At the moment,
    /// everything up to (and including) the colon is removed and discarded, 
    /// so not specifying 'key:' works as well
    QString specifier = QString::fromUtf8(hookSpec);
    //specifier.count(QRegExp("[:]"));
    specifier.replace(QRegExp(".*[:]"), "");
    QKeySequence requested = QKeySequence::fromString(specifier, QKeySequence::PortableText);

    /// Go through all shortcuts, see if one exists, if not create it
    int numberShortCuts = m_shortCut.count();
    bool taken = false;
    for(int i=0; i<numberShortCuts; i++)
    {
        if(m_hookSpec.at(i)==requested)
        {
            taken = true;
            latestHookIndex = i;
            m_hooksSignedUp[i]+=1;
            // in case it was in use earlier and got disabled in removeShortCut
            m_shortCut.at(i)->setEnabled(true);
        }
    }

    if(!taken)
    {
        m_hookSpec.append(requested);
        m_shortCut.append(new QShortcut(m_hookSpec.at(numberShortCuts), qobject_cast<QWidget*>(this->parent())));
        latestHookIndex = numberShortCuts;
        m_hooksSignedUp.append(1);
    }
}


void hookEvent::removeShortCut(int whichShortCut)
{
    if(m_hooksSignedUp.at(whichShortCut)==1)
    {
        m_shortCut.at(whichShortCut)->setEnabled(false);
        m_hooksSignedUp[whichShortCut]=0;
    }
}
