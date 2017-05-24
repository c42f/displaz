// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "IpcChannel.h"

#include <cstdint>

#include <QCoreApplication>
#include <QElapsedTimer>

#include "util.h"
#include "qtutil.h"

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
    QElapsedTimer timer;
    timer.start();
    qint64 currTimeout = timeoutMsecs;
    while (true)
    {
        socket->connectToServer(serverName);
        if (socket->waitForConnected(currTimeout))
            return std::unique_ptr<IpcChannel>(new IpcChannel(socket.release()));
        if (timeoutMsecs >= 0)
            currTimeout = timeoutMsecs - timer.elapsed();
        if (socket->error() == QLocalSocket::SocketTimeoutError)
        {
            return std::unique_ptr<IpcChannel>();
        }
        else if (socket->error() == QLocalSocket::UnknownSocketError)
        {
            // "Unknown" errors on linux may result in the socket getting into
            // a bad state from which it never recovers.  Make a new one.
            socket.reset(new QLocalSocket());
        }
        // For maximum robustness, consider all other errors to be retryable.
        //
        // Some errors such as ServerNotFoundError may seem fatal, but a race
        // condition may occur where the remote instance has only just started
        // and hasn't created the socket yet.  Prevent busy looping on these
        // kinds of conditions by including a small sleep between retries.
        milliSleep(1);
    }
}

QByteArray IpcChannel::receiveMessage(int timeoutMsecs)
{
    disconnect(m_socket, SIGNAL(readyRead()), 0, 0);
#   ifdef _WIN32
    // Win32 workaround - see waitForDisconnected()
    QElapsedTimer timer;
    timer.start();
    while (m_socket->state() != QLocalSocket::UnconnectedState &&
           (timeoutMsecs == -1 || timer.elapsed() < timeoutMsecs))
    {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        milliSleep(1);
        if (appendCurrentMessage())
        {
            QByteArray msg = m_message;
            clearCurrentMessage();
            return msg;
        }
    }
#   else
    while (m_socket->waitForReadyRead(timeoutMsecs))
    {
        if (appendCurrentMessage())
        {
            QByteArray msg = m_message;
            clearCurrentMessage();
            return msg;
        }
    }
#   endif
    throw DisplazError("Could not read message from socket: %s",
                       m_socket->errorString());
}

void IpcChannel::sendMessage(const QByteArray& message)
{
    QDataStream stream(m_socket);
    stream << message.length();
    stream.writeRawData(message.data(), message.length());
    m_socket->flush();
}

bool IpcChannel::disconnectFromServer(int timeoutMsecs)
{
    m_socket->disconnectFromServer();
    return waitForDisconnected(timeoutMsecs);
}


bool IpcChannel::waitForDisconnected(int timeoutMsecs)
{
#   ifdef _WIN32
    // Aaaarghhh.  QLocalSocket::waitForDisconnected() seems like it's meant to
    // be a synchronous version for use without an event loop.  However,
    // qt-4.8.6 on windows (and it appears many other versions, including
    // latest qt-5) have a bug in waitForDisconnected(): it uses a separate
    // thread to do the actual write via an internal QWindowsPipeWriter.
    // QLocalSocket is notified when the data has has been written via a
    // connection, but this is a QueuedConnection due to being in a separate
    // thread, so the signal never gets seen unless an event loop is used.
    QElapsedTimer timer;
    timer.start();
    while (m_socket->state() != QLocalSocket::UnconnectedState &&
           (timeoutMsecs == -1 || timer.elapsed() < timeoutMsecs))
    {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        milliSleep(1); // Crude, but whatever.  This is a workaround.
    }
    return m_socket->state() == QLocalSocket::UnconnectedState;
#   else
    if (m_socket->state() == QLocalSocket::UnconnectedState)
        return true;
    return m_socket->waitForDisconnected(timeoutMsecs);
#   endif
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
