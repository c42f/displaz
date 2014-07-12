#include "logger.h"

Logger g_logger;


//------------------------------------------------------------------------------
StreamLogger::StreamLogger(std::ostream& outStream)
    : m_prevPrintWasProgress(false),
    m_prevProgressFraction(-100),
    m_out(outStream)
{ }

StreamLogger::~StreamLogger()
{
    if (m_prevPrintWasProgress)
        m_out << "\n";
}

void StreamLogger::logImpl(LogLevel level, const std::string& msg)
{
    if (m_prevPrintWasProgress)
        tfm::format(m_out, "\n");
    m_prevPrintWasProgress = false;
    if (level == Progress)
    {
        m_progressPrefix = msg;
        progressImpl(0);
    }
    else if (level == Info)
        tfm::format(m_out, "%s\n", msg);
    else if (level == Warning)
        tfm::format(m_out, "WARNING: %s\n", msg);
    else if (level == Error)
        tfm::format(m_out, "ERROR: %s\n", msg);
}

void StreamLogger::progressImpl(double progressFraction)
{
    const int barFullWidth = std::max(10, 60 - 3 - (int)m_progressPrefix.size());
    const int barFraction = (int)floor(barFullWidth*std::min(1.0, std::max(0.0, progressFraction)) + 0.5);
    if (fabs(progressFraction - m_prevProgressFraction) > 0.01)
    {
        m_prevProgressFraction = progressFraction;
        tfm::format(m_out, "%s [%s%s]\r", m_progressPrefix,
                    std::string(barFraction, '='),
                    std::string(barFullWidth - barFraction, ' '));
        m_prevPrintWasProgress = true;
    }
}


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
