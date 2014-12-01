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
            appendPlainText(msg);
            break;
    }
    ensureCursorVisible();
}


//------------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& out, const QByteArray& s)
{
    out.write(s.constData(), s.size());
    return out;
}


std::ostream& operator<<(std::ostream& out, const QString& s)
{
    out << s.toUtf8();
    return out;
}
