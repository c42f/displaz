// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "qtlogger.h"

QtLogger g_logger;


//------------------------------------------------------------------------------
LogViewer::LogViewer(QWidget* parent)
    : QPlainTextEdit(parent)
{ }


void LogViewer::connectLogger(QtLogger* logger)
{
    connect(logger, SIGNAL(logMessage(int, QString)),
            this, SLOT(appendLogMessage(int, QString)),
            Qt::QueuedConnection);
}


void LogViewer::appendLogMessage(int logLevel, QString msg)
{
    moveCursor(QTextCursor::End);
    switch (logLevel)
    {
        case Logger::Warning:
            appendHtml("<b>WARNING</b>: ");
            insertPlainText(msg);
            break;
        case Logger::Error:
            appendHtml("<b>ERROR</b>: ");
            insertPlainText(msg);
            break;
        case Logger::Info:
            appendHtml("<b>INFO</b>: ");
            insertPlainText(msg);
            break;
    }
    ensureCursorVisible();
}

