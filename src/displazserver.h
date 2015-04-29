// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef DISPLAZ_SERVER_H_INCLUDED
#define DISPLAZ_SERVER_H_INCLUDED

#include <iostream>
#include <memory>

#include <QtCore/QDataStream>
#include <QtNetwork/QLocalServer>
#include <QtNetwork/QLocalSocket>

#include "util.h"

/// Server which listens for incoming messages on a local socket
class DisplazServer : public QObject
{
    Q_OBJECT
    public:
        DisplazServer(QString socketName, QObject* parent = NULL)
            : QObject(parent)
        {
            m_server = new QLocalServer(this);
            if (!QLocalServer::removeServer(socketName))
                qWarning("Could not clean up socket file \"%s\"", qPrintable(socketName));
            if (!m_server->listen(socketName))
                qWarning("Could not listen on socket \"%s\"", qPrintable(socketName));
            connect(m_server, SIGNAL(newConnection()),
                    this, SLOT(newConnection()));
        }

        static QString socketName(QString suffix)
        {
            QString currUid = QString::fromStdString(currentUserUid());
            QString name = "displaz-ipc-" + currUid;
            if (!suffix.isEmpty())
                name += "-" + suffix;
            return name;
        }

    signals:
        /// Triggered when a message arrives
        void messageReceived(QByteArray msg);

    private slots:
        void newConnection()
        {
            // Note - socket is a child of m_server but we may clean it up when
            // we're done with it, so stuff it into a unique_ptr
            std::unique_ptr<QLocalSocket> socket(m_server->nextPendingConnection());
            // Wait until we have enough bytes to read the message length
            while (socket->bytesAvailable() < (int)sizeof(quint32))
            {
                // Note that isValid() can return true even when the socket is
                // disconnected - protect against this with extra checks.
                if (!socket->isValid() ||
                    (socket->state() == QLocalSocket::UnconnectedState &&
                     socket->bytesAvailable() < (int)sizeof(quint32)))
                {
                    std::cerr << "Socket became invalid or disconnected "
                                 "while waiting for message length\n";
                    return;
                }
                socket->waitForReadyRead(1000);
            }
            quint32 msgLen = 0;
            QDataStream stream(socket.get());
            stream >> msgLen;
            if (msgLen == 0)
                return;
            // Read actual message
            QByteArray msg(msgLen, '\0');
            quint32 bytesRead = 0;
            do
            {
                int n = stream.readRawData(msg.data() + bytesRead, msgLen - bytesRead);
                if (n < 0)
                {
                    std::cerr << "Error while reading socket data\n";
                    return;
                }
                bytesRead += n;
            }
            while (bytesRead < msgLen && socket->waitForReadyRead(1000));
            if (bytesRead < msgLen)
            {
                std::cerr << "Socket message truncated - ignoring\n";
                return;
            }
            emit messageReceived(msg);
        }

    private:
        QLocalServer* m_server;
};


#endif // DISPLAZ_SERVER_H_INCLUDED
