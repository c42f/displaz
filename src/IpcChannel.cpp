// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "IpcChannel.h"
#include "util.h"
#include "qtlogger.h"


IpcChannel::IpcChannel(QLocalSocket* socket, QObject* parent)
    : QObject(parent),
    m_socket(socket),
    m_messageSize(0)
{
    m_socket->setParent(this);
    connect(m_socket, SIGNAL(readyRead()), this, SLOT(readReadyData()));
    connect(m_socket, SIGNAL(disconnected()), this, SLOT(handleDisconnect()));
    connect(m_socket, SIGNAL(error(QLocalSocket::LocalSocketError)),
            this, SLOT(handleError(QLocalSocket::LocalSocketError)));
}

std::unique_ptr<IpcChannel> IpcChannel::connectToServer(QString serverName, int timeoutMsecs)
{
    std::unique_ptr<QLocalSocket> socket(new QLocalSocket());
    socket->connectToServer(serverName);
    if (!socket->waitForConnected(timeoutMsecs))
        return std::unique_ptr<IpcChannel>();
    return std::unique_ptr<IpcChannel>(new IpcChannel(socket.release()));
}

QByteArray IpcChannel::receiveMessage(int timeoutMsecs)
{
    disconnect(m_socket, SIGNAL(readyRead()), 0, 0);
    while (m_socket->waitForReadyRead(timeoutMsecs))
    {
        if (appendCurrentMessage())
        {
            QByteArray msg = m_message;
            clearCurrentMessage();
            return msg;
        }
    }
    throw DisplazError("Could not read message from socket: %s",
                       m_socket->errorString());
    connect(m_socket, SIGNAL(readyRead()), this, SLOT(readReadyData()));
}

void IpcChannel::sendMessage(const QByteArray& message)
{
    QDataStream stream(m_socket);
    stream << message.length();
    stream.writeRawData(message.data(), message.length());
    m_socket->flush();
}

void IpcChannel::disconnectFromServer(int timeoutMsecs)
{
    m_socket->disconnectFromServer();
    if (m_socket->state() != QLocalSocket::UnconnectedState)
        m_socket->waitForDisconnected(timeoutMsecs);
}


//--------------------------------------------------
// Private functions

/// Read and emit messages until data runs out
void IpcChannel::readReadyData()
{
    while (appendCurrentMessage())
    {
        emit messageReceived(m_message);
        clearCurrentMessage();
    }
}

/// Log error messages
void IpcChannel::handleError(QLocalSocket::LocalSocketError errorCode)
{
    if (errorCode == QLocalSocket::PeerClosedError)
        return;
    qWarning() << "Socket failure " << errorCode << ": " << m_socket->errorString();
}

void IpcChannel::handleDisconnect()
{
    disconnect(m_socket, SIGNAL(error(QLocalSocket::LocalSocketError)),
                this, SLOT(handleError(QLocalSocket::LocalSocketError)));
    if (!m_message.isEmpty())
        qWarning() << "Ignoring partial message: " << m_message;
    emit disconnected();
}

/// Append bytes to current message from socket.  Return true if
/// m_message is complete.
bool IpcChannel::appendCurrentMessage()
{
    qint64 avail = m_socket->bytesAvailable();
    if (m_messageSize == 0)
    {
        if (avail < (qint64)sizeof(quint32))
            return false;
        QDataStream stream(m_socket);
        stream >> m_messageSize;
    }
    quint32 toRead = (quint32)std::min<qint64>(m_messageSize - m_message.length(), avail);
    m_message.append(m_socket->read(toRead));
    if (m_message.length() == (int)m_messageSize)
    {
        //qWarning() << "IPC:" << m_message;
        return true;
    }
    return false;
}

/// Reset current message and make ready for next message from socket
void IpcChannel::clearCurrentMessage()
{
    m_message.clear();
    m_messageSize = 0;
}
