#ifndef LOGGER_H_INCLUDED
#define LOGGER_H_INCLUDED

#include "tinyformat.h"

#include <QObject>
#include <QPlainTextEdit>
#include <QString>


//------------------------------------------------------------------------------
/// Logger class for log message formatting using printf-style strings
class Logger : public QObject
{
    Q_OBJECT
    public:
        enum LogLevel
        {
            Info,
            Warning,
            Error
        };

        /// Printf-style logging functions (typesafe via tinyformat)
#       define DISPLAZ_MAKE_LOG_FUNCS(n)                                   \
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
        void info   (const char* fmt) { log(Info, tfm::format(fmt)); }
        void warning(const char* fmt) { log(Warning, tfm::format(fmt)); }
        void error  (const char* fmt) { log(Error, tfm::format(fmt)); }

        /// Log message at the given log level
        virtual void log(LogLevel level, const std::string& msg)
        {
            emit logMessage(level, QString::fromUtf8(msg.c_str()));
        }

    signals:
        /// Signal emitted every time a log message comes in
        void logMessage(int logLevel, QString msg);

    private:
        static Logger* m_instance;
};


/// Global logger instance
extern Logger g_logger;


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
        void connectLogger(Logger* logger);

        /// Append plain text message to the running log
        void appendLogMessage(int logLevel, QString msg);
};


//------------------------------------------------------------------------------
/// Print QByteArray to std stream as raw data
std::ostream& operator<<(std::ostream& out, const QByteArray& s);

/// Print QString to std stream as utf8
std::ostream& operator<<(std::ostream& out, const QString& s);


#endif // LOGGER_H_INCLUDED
