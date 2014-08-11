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
