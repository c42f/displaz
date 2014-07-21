#ifndef LOGGER_H_INCLUDED
#define LOGGER_H_INCLUDED

#include "tinyformat.h"

#include <QObject>
#include <QPlainTextEdit>
#include <QString>


//------------------------------------------------------------------------------
/// Logger class for log message formatting using printf-style strings
class Logger
{
    public:
        enum LogLevel
        {
            Progress,
            Info,
            Warning,
            Error
        };

        /// Printf-style logging functions (typesafe via tinyformat)
#       define DISPLAZ_MAKE_LOG_FUNCS(n)                                   \
                                                                           \
        template<TINYFORMAT_ARGTYPES(n)>                                   \
        void progress(const char* fmt, TINYFORMAT_VARARGS(n))              \
        {                                                                  \
            log(Progress, tinyformat::format(fmt, TINYFORMAT_PASSARGS(n)));\
        }                                                                  \
                                                                           \
        template<TINYFORMAT_ARGTYPES(n)>                                   \
        void info(const char* fmt, TINYFORMAT_VARARGS(n))                  \
        {                                                                  \
            log(Info, tinyformat::format(fmt, TINYFORMAT_PASSARGS(n)));    \
        }                                                                  \
                                                                           \
        template<TINYFORMAT_ARGTYPES(n)>                                   \
        void warning(const char* fmt, TINYFORMAT_VARARGS(n))               \
        {                                                                  \
            log(Warning, tinyformat::format(fmt, TINYFORMAT_PASSARGS(n))); \
        }                                                                  \
                                                                           \
        template<TINYFORMAT_ARGTYPES(n)>                                   \
        void error(const char* fmt, TINYFORMAT_VARARGS(n))                 \
        {                                                                  \
            log(Error, tinyformat::format(fmt, TINYFORMAT_PASSARGS(n)));   \
        }

        TINYFORMAT_FOREACH_ARGNUM(DISPLAZ_MAKE_LOG_FUNCS)
#       undef DISPLAZ_MAKE_LOG_FUNCS

        // 0-arg versions
        void progress (const char* fmt) { log(Progress, tfm::format(fmt)); }
        void info     (const char* fmt) { log(Info, tfm::format(fmt)); }
        void warning  (const char* fmt) { log(Warning, tfm::format(fmt)); }
        void error    (const char* fmt) { log(Error, tfm::format(fmt)); }

        /// Log message at the given log level
        virtual void log(LogLevel level, const std::string& msg) = 0;

        /// Report progress of some processing step
        virtual void progress(double progressFraction) = 0;
};


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


/// Logger class which logs to the console
class StreamLogger : public Logger
{
    public:
        StreamLogger(std::ostream& outStream);
        ~StreamLogger();

        virtual void log(LogLevel level, const std::string& msg);
        virtual void progress(double progressFraction);

    private:
        bool m_prevPrintWasProgress;
        double m_prevProgressFraction;
        std::string m_progressPrefix;
        std::ostream& m_out;
};


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


#endif // LOGGER_H_INCLUDED
