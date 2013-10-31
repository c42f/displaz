// Copyright (C) 2012, Chris J. Foster and the other authors and contributors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of the software's owners nor the names of its
//   contributors may be used to endorse or promote products derived from this
//   software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// (This is the BSD 3-clause license)

#ifndef DISPLAZ_SERVER_H_INCLUDED
#define DISPLAZ_SERVER_H_INCLUDED

#include <iostream>
#include <memory>

#include <QtCore/QDataStream>
#include <QtNetwork/QLocalServer>
#include <QtNetwork/QLocalSocket>


/// Server which listens for incoming messages on a local socket
class DisplazServer : public QObject
{
    Q_OBJECT
    public:
        DisplazServer(QString socketName, QObject* parent = NULL)
            : QObject(parent)
        {
            m_server = new QLocalServer(this);
            m_server->listen(socketName);
            connect(m_server, SIGNAL(newConnection()),
                    this, SLOT(newConnection()));
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
                if (!socket->isValid())
                {
                    std::cerr << "Socket became invalid waiting for message length\n";
                    return;
                }
                socket->waitForReadyRead(1000);
            }
            quint32 msgLen = 0;
            QDataStream stream(socket.get());
            stream >> msgLen;
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
            } while (bytesRead < msgLen && socket->waitForReadyRead(1000));
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
