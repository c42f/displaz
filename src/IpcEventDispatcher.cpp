// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "IpcEventDispatcher.h"
#include "IpcChannel.h"
#include "PointViewerMainWindow.h"


IpcEventDispatcher::IpcEventDispatcher(QObject* parent, PointViewerMainWindow* getPayload, 
                                       QByteArray hookSpec, QByteArray hookPayload, int whichShortCut)
    : QObject(parent),
    m_hookSpec(),
    m_hookPayload()
{
    m_getPayload = getPayload;
    m_hookSpec = hookSpec;
    m_hookPayload = hookPayload;
    m_whichShortCut = whichShortCut;
    connect(this, SIGNAL(sendIpcMessage(QByteArray)), parent, SLOT(sendMessage(QByteArray)));
    connect(parent, SIGNAL(disconnected()), this, SLOT(removedHook()));
}

// Get payload and dispatch signal to IpcChannel
void IpcEventDispatcher::hookActivated()
{
    QByteArray hookPayload = m_getPayload->hookPayload(m_hookPayload);
    QByteArray message = m_hookSpec + " " + hookPayload + "\n";

    emit sendIpcMessage(message);
}
