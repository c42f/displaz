// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include <memory>

#include <QDataStream>
#include <QByteArray>
#include <QLocalSocket>


/// Communication channel for local message-based IPC
///
/// The main idea here is to wrap QLocalSocket so that only complete messages
/// are delivered.
class IpcChannel : public QObject
{
    Q_OBJECT
    public:
        /// Create IPC channel from open socket connection
        ///
        /// socket is parented to the newly created channel
        IpcChannel(QLocalSocket* socket, QObject* parent = NULL);

        /// Connect to a local socket with the given name.
        ///
        /// If the connection succeeds within the given timeout, return an open
        /// channel, otherwise return null.
        ///
        /// serverName -   Socket name as passed to QLocalSocket
        /// timeoutMsecs - Timeout in milliseconds.  If negative, try forever.
        static std::unique_ptr<IpcChannel> connectToServer(
                QString serverName, int timeoutMsecs = 10000);

        /// Read next message synchronously
        ///
        /// Blocks until the message is received.  If the timeout is reached,
        /// or the socket disconnects, a DisplazError is thrown.
        ///
        /// The messageReceived signal is disabled for the message returned
        QByteArray receiveMessage(int timeoutMsecs = 10000);

        /// Synchronous disconnect from server
        ///
        /// If the connection was disconnected, return true.
        bool disconnectFromServer(int timeoutMsecs = 10000);

        /// Synchronous wait for server to disconnect
        ///
        /// If the connection was disconnected, return true.
        ///
        /// If timeoutMsecs is -1, wait indefinitely.
        bool waitForDisconnected(int timeoutMsecs = 10000);

    public slots:
        /// Send message asynchronously
        ///
        /// The message is formatted on the wire as a big endian uint32 length
        /// followed by the raw message bytes.
        void sendMessage(const QByteArray& message);

    signals:
        /// Triggered when a message arrives
        void messageReceived(QByteArray message);

        /// Triggered when disconnected from the server
        void disconnected();


    private slots:
        void readReadyData();
        void handleError(QLocalSocket::LocalSocketError errorCode);
        void handleDisconnect();

    private:
        bool appendCurrentMessage();
        void clearCurrentMessage();

        QByteArray m_message;
        QLocalSocket* m_socket;
        quint32 m_messageSize;
};


