// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef LOGGER_H_INCLUDED
#define LOGGER_H_INCLUDED

#include "tinyformat.h"

//------------------------------------------------------------------------------
/// Logger class for log message formatting using printf-style strings
class Logger
{
    public:
        enum LogLevel
        {
            Error,
            Warning,
            Info,
            Debug,
            Progress
        };

        Logger(LogLevel logLevel = Info, bool logProgress = true)
            : m_logLevel(logLevel), m_logProgress(logProgress) { }

        void setLogLevel(LogLevel logLevel) { m_logLevel = logLevel; }
        void setLogProgress(bool logProgress) { m_logProgress = logProgress; }

        /// Printf-style logging functions (typesafe via tinyformat)
#       define DISPLAZ_MAKE_LOG_FUNCS(n)                                   \
                                                                           \
        template<TINYFORMAT_ARGTYPES(n)>                                   \
        void progress(const char* fmt, TINYFORMAT_VARARGS(n))              \
        {                                                                  \
            if (m_logProgress)                                             \
                log(Progress, tfm::format(fmt, TINYFORMAT_PASSARGS(n)));   \
        }                                                                  \
                                                                           \
        template<TINYFORMAT_ARGTYPES(n)>                                   \
        void debug(const char* fmt, TINYFORMAT_VARARGS(n))                 \
        {                                                                  \
            if (m_logLevel >= Debug)                                       \
                log(Debug, tfm::format(fmt, TINYFORMAT_PASSARGS(n)));      \
        }                                                                  \
                                                                           \
        template<TINYFORMAT_ARGTYPES(n)>                                   \
        void info(const char* fmt, TINYFORMAT_VARARGS(n))                  \
        {                                                                  \
            if (m_logLevel >= Info)                                        \
                log(Info, tfm::format(fmt, TINYFORMAT_PASSARGS(n)));       \
        }                                                                  \
                                                                           \
        template<TINYFORMAT_ARGTYPES(n)>                                   \
        void warning(const char* fmt, TINYFORMAT_VARARGS(n))               \
        {                                                                  \
            if (m_logLevel >= Warning)                                     \
                log(Warning, tfm::format(fmt, TINYFORMAT_PASSARGS(n)));    \
        }                                                                  \
                                                                           \
        template<TINYFORMAT_ARGTYPES(n)>                                   \
        void error(const char* fmt, TINYFORMAT_VARARGS(n))                 \
        {                                                                  \
            if (m_logLevel >= Error)                                       \
                log(Error, tfm::format(fmt, TINYFORMAT_PASSARGS(n)));      \
        }

        TINYFORMAT_FOREACH_ARGNUM(DISPLAZ_MAKE_LOG_FUNCS)
#       undef DISPLAZ_MAKE_LOG_FUNCS

        // 0-arg versions
        void progress (const char* fmt) { log(Progress, tfm::format(fmt)); }
        void debug    (const char* fmt) { log(Debug, tfm::format(fmt)); }
        void info     (const char* fmt) { log(Info, tfm::format(fmt)); }
        void warning  (const char* fmt) { log(Warning, tfm::format(fmt)); }
        void error    (const char* fmt) { log(Error, tfm::format(fmt)); }

        /// Log message at the given log level
        virtual void log(LogLevel level, const std::string& msg) = 0;

        /// Report progress of some processing step
        void progress(double progressFraction)
        {
            if (m_logProgress)
                progressImpl(progressFraction);
        }

    protected:
        virtual void progressImpl(double progressFraction) = 0;

    private:
        LogLevel m_logLevel;
        bool m_logProgress;
};


/// Logger class which logs to the console
class StreamLogger : public Logger
{
    public:
        StreamLogger(std::ostream& outStream);
        ~StreamLogger();

        virtual void log(LogLevel level, const std::string& msg);

    protected:
        virtual void progressImpl(double progressFraction);

    private:
        bool m_prevPrintWasProgress;
        double m_prevProgressFraction;
        std::string m_progressPrefix;
        std::ostream& m_out;
};


#endif // LOGGER_H_INCLUDED
