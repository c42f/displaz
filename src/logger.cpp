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

#include "logger.h"

#include <cmath>


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

void StreamLogger::log(LogLevel level, const std::string& msg)
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


