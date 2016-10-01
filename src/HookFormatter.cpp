// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "HookFormatter.h"

#include "IpcChannel.h"
#include "PointViewerMainWindow.h"


HookFormatter::HookFormatter(PointViewerMainWindow* getPayload, QByteArray hookSpec,
                                       QByteArray hookPayload, QObject* parent)
    : QObject(parent),
    m_hookSpec(hookSpec),
    m_hookPayload(hookPayload),
    m_getPayload(getPayload),
    m_eventId(0)
{
    connect(this, SIGNAL(sendIpcMessage(QByteArray)), parent, SLOT(sendMessage(QByteArray)));
    connect(parent, SIGNAL(disconnected()), this, SLOT(channelDisconnected()));
}

/// Get payload and dispatch signal to IpcChannel
void HookFormatter::hookActivated()
{
    QByteArray hookPayload = m_getPayload->hookPayload(m_hookPayload);
    QByteArray message = m_hookSpec + " " + hookPayload + "\n";

    emit sendIpcMessage(message);
}
