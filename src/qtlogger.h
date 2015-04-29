// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

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

    signals:
        /// Signal emitted every time a log message comes in
        void logMessage(int logLevel, QString msg);

        /// Signal emitted when processing progress has been made
        void progressPercent(int percent);

    protected:
        virtual void progressImpl(double progressFraction)
        {
            emit progressPercent(int(100*progressFraction));
        }
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
