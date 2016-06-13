// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "HookManager.h"

#include <QRegExp>

#include "HookFormatter.h"

HookManager::HookManager(QObject* parent)
    : QObject(parent),
    m_event(),
    m_eventSpec(),
    m_hooksSignedUp()
{
}


void HookManager::connectHook(QByteArray eventSpec, HookFormatter* formatter)
{
    /// Remove keyword before key sequence, indicated by colon. At the moment,
    /// everything up to (and including) the colon is removed and discarded, 
    /// so not specifying 'key:' works as well.
    QString specifier = QString::fromUtf8(eventSpec);
    //specifier.count(QRegExp("[:]"));
    specifier.replace(QRegExp(".*[:]"), "");
    QKeySequence requested = QKeySequence::fromString(specifier, QKeySequence::PortableText);

    /// Go through all shortcuts, see if one exists, if not create it and connect it to HookFormatter
    int numberEvents = m_event.count();
    bool taken = false;
    for(int i=0; i<numberEvents; i++)
    {
        if(m_eventSpec.at(i)==requested)
        {
            taken = true;
            m_hooksSignedUp[i]+=1;
            /// In case it was in use earlier and got disabled in deactivateEvent.
            m_event.at(i)->setEnabled(true);
            connect(m_event.at(i), SIGNAL(activated()), formatter, SLOT(hookActivated()));
            formatter->setEventId(i);
        }
    }

    if(!taken)
    {
        m_eventSpec.append(requested);
        m_event.append(new QShortcut(m_eventSpec.at(numberEvents), qobject_cast<QWidget*>(this->parent())));
        m_hooksSignedUp.append(1);
        connect(m_event.at(numberEvents), SIGNAL(activated()), formatter, SLOT(hookActivated()));
        formatter->setEventId(numberEvents);
    }

    connect(formatter, SIGNAL(hookRemoved(int)), this, SLOT(deactivateEvent(int)));
}


void HookManager::deactivateEvent(int eventId)
{
    m_hooksSignedUp[eventId]--;

    if(m_hooksSignedUp.at(eventId) == 0)
        m_event.at(eventId)->setEnabled(false);
}
