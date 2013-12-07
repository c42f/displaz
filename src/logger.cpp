#include "logger.h"

Logger g_logger;


//------------------------------------------------------------------------------
LogViewer::LogViewer(QWidget* parent)
    : QPlainTextEdit(parent)
{ }


void LogViewer::connectLogger(Logger* logger)
{
    connect(logger, SIGNAL(logMessage(int, QString)),
            this, SLOT(appendLogMessage(int, QString)),
            Qt::QueuedConnection);
}


void LogViewer::appendLogMessage(int logLevel, QString msg)
{
    switch (logLevel)
    {
        case Logger::Warning:
            appendHtml("<b>WARNING</b>: " + msg);
            break;
        case Logger::Error:
            appendHtml("<b>ERROR</b>: " + msg);
            break;
        case Logger::Info:
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
