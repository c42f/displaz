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

#ifndef QT_LOGGER_H_INCLUDED
#define QT_LOGGER_H_INCLUDED

#include <QObject>
#include <QPlainTextEdit>
#include <QString>

#include "logger.h"

/// Logger class which logs output to qt signals
class QtLogger : public QObject, public Logger
{
    Q_OBJECT
    public:
        QtLogger(QObject* parent = 0) : QObject(parent) {}

        virtual void log(LogLevel level, const std::string& msg)
        {
            emit logMessage(level, QString::fromUtf8(msg.c_str()));
        }

        virtual void progress(double progressFraction)
        {
            emit progressPercent(int(100*progressFraction));
        }

    signals:
        /// Signal emitted every time a log message comes in
        void logMessage(int logLevel, QString msg);

        /// Signal emitted when processing progress has been made
        void progressPercent(int percent);
};


/// Global displaz logger instance
extern QtLogger g_logger;


//------------------------------------------------------------------------------
/// Viewer widget for log messages.
///
/// This is intended to work together with the Logger class as a frontend which
/// does to the actual log message formatting.  The logger frontend is
/// threadsafe, but LogViewer must run on the GUI thread.
class LogViewer : public QPlainTextEdit
{
    Q_OBJECT
    public:
        LogViewer(QWidget* parent = 0);

    public slots:
        /// Connect given logger to the viewer.
        ///
        /// Note that for thread safety this must be a queued connection, hence
        /// the special purpose method here.
        void connectLogger(QtLogger* logger);

        /// Append plain text message to the running log
        void appendLogMessage(int logLevel, QString msg);
};


//------------------------------------------------------------------------------
/// Print QByteArray to std stream as raw data
std::ostream& operator<<(std::ostream& out, const QByteArray& s);

/// Print QString to std stream as utf8
std::ostream& operator<<(std::ostream& out, const QString& s);


#endif // QT_LOGGER_H_INCLUDED
