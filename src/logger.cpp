// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "logger.h"

#include <cmath>

//------------------------------------------------------------------------------
void Logger::log(LogLevel level, const char* fmt, tfm::FormatListRef flist)
{
    if (level > m_logLevel)
        return;
    std::ostringstream ss;
    tfm::vformat(ss, fmt, flist);
    logImpl(level, ss.str());
}


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
        progress(0.0);
    }
    else if (level == Debug)
        tfm::format(m_out, "DEBUG: %s\n", msg);
    else if (level == Info)
        tfm::format(m_out, "%s\n", msg);
    else if (level == Warning)
        tfm::format(m_out, "WARNING: %s\n", msg);
    else if (level == Error)
        tfm::format(m_out, "ERROR: %s\n", msg);
}

void StreamLogger::progressImpl(double progressFraction)
{
    if (fabs(progressFraction - m_prevProgressFraction) < 0.01)
        return;
    const int barFullWidth = std::max(10, 60 - 3 - (int)m_progressPrefix.size());
    const int barFraction = (int)floor(barFullWidth*std::min(1.0, std::max(0.0, progressFraction)) + 0.5);
    m_prevProgressFraction = progressFraction;
    tfm::format(m_out, "%s [%s%s]\r", m_progressPrefix,
                std::string(barFraction, '='),
                std::string(barFullWidth - barFraction, ' '));
    m_prevPrintWasProgress = true;
}


