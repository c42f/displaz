// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "HookManager.h"

#include <QRegExp>

#include "util.h"
#include "HookFormatter.h"
#include "IpcChannel.h"

HookManager::HookManager(QObject* parent)
    : QObject(parent),
    m_events(),
    m_eventSpecs(),
    m_hooksSignedUp()
{
}


void HookManager::connectHook(QByteArray eventSpec, HookFormatter* formatter)
{
    /// Remove keyword before key sequence, indicated by colon, check if
    /// valid specifier.
    QStringList specifier =  QString::fromUtf8(eventSpec).split(":");

    if(specifier.count() != 2)
    {
        IpcChannel* channel = dynamic_cast<IpcChannel*>(formatter->parent());
        if (channel) channel->sendMessage(QByteArray("Error: Hook event specifier must be of the form 'key:x+y'.\n"));
        delete formatter;
        return;
    }
    else if(specifier[0] != "key")
    {
        IpcChannel* channel = dynamic_cast<IpcChannel*>(formatter->parent());
        if (channel) channel->sendMessage(QByteArray("Error: Hook event specifier must be of the form 'key:x+y'.\n"));
        delete formatter;
        return;
    }


    QKeySequence requestedSpec = QKeySequence::fromString(specifier[1], QKeySequence::PortableText);

    /// Go through all shortcuts, see if one exists, if not create it and connect it to HookFormatter
    int numberEvents = m_events.count();
    bool taken = false;
    for(int i=0; i<numberEvents; i++)
    {
        if(m_eventSpecs[i] == requestedSpec)
        {
            taken = true;
            m_hooksSignedUp[i]+=1;
            /// In case it was in use earlier and got disabled in deactivateEvent.
            m_events[i]->setEnabled(true);
            connect(m_events[i], SIGNAL(activated()), formatter, SLOT(hookActivated()));
            formatter->setEventId(i);
        }
    }

    if(!taken)
    {
        m_eventSpecs.append(requestedSpec);
        m_events.append(new QShortcut(m_eventSpecs[numberEvents], qobject_cast<QWidget*>(this->parent())));
        m_hooksSignedUp.append(1);
        connect(m_events[numberEvents], SIGNAL(activated()), formatter, SLOT(hookActivated()));
        formatter->setEventId(numberEvents);
    }

    connect(formatter, SIGNAL(hookRemoved(int)), this, SLOT(deactivateEvent(int)));
}


void HookManager::deactivateEvent(int eventId)
{
    if((eventId < m_hooksSignedUp.count()) && (eventId>=0))
    {
        m_hooksSignedUp[eventId]--;
        if(m_hooksSignedUp[eventId] == 0)
            m_events[eventId]->setEnabled(false);
    }
    else
        std::cerr << "Error: Requested deletion of hook out of range.\n";
}
