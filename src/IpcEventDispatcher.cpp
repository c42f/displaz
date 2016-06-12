// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "IpcEventDispatcher.h"
#include "IpcChannel.h"
#include "PointViewerMainWindow.h"


IpcEventDispatcher::IpcEventDispatcher(QObject* parent, PointViewerMainWindow* getPayload, QByteArray hookSpec, QByteArray hookPayload)
    : QObject(parent),
    m_hookSpec(),
    m_hookPayload()
{
    connect(this, SIGNAL(sendIpcMessage(QByteArray)), parent, SLOT(sendMessage(QByteArray)), Qt::QueuedConnection);
    m_getPayload = getPayload;
    m_hookSpec = QKeySequence::fromString(QString::fromUtf8(hookSpec), QKeySequence::PortableText);
    m_hookPayload = hookPayload;
}

// Get payload and dispatch signal to IpcChannel
void IpcEventDispatcher::hookActivated()
{
    QByteArray hookPayload = m_getPayload->hookPayload(m_hookPayload);
    QByteArray message = m_hookSpec.toString(QKeySequence::PortableText).toUtf8() + " "
                        + hookPayload + "\n";

    emit sendIpcMessage(message);
}
