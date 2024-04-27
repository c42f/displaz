#include "FrameRate.h"

#include <iostream>

#include <tinyformat.h>

FrameRate::FrameRate()
{
    m_timer.start();
}

FrameRate &
FrameRate::operator++()
{
    m_frames++;
    m_buffer.push_back(m_timer.elapsed());

    // Recalculate frame rate and frame time
    // if we have sufficient samples and
    // enough time has elapsed.

    if (m_buffer.size() >= 2)
    {
        if (m_buffer.back() - m_lastCalc >= m_step)
        {
            m_lastCalc      = m_buffer.back();
            m_lastFrameTime = double(m_buffer.back() - m_buffer.front()) / (1000.0 * (m_buffer.size()-1));
            m_lastFrameRate = 1.0 / m_lastFrameTime;
            m_buffer.clear();

            std::cout << detailed() << std::endl;
        }
    }

    return *this;
}

double FrameRate::elapsedTime() const { return m_timer.elapsed() / 1000.0; }
size_t FrameRate::totalFrames() const { return m_frames; }

double FrameRate::frameRate() const { return m_lastFrameRate; }
double FrameRate::frameTime() const { return m_lastFrameTime; }

std::string
FrameRate::summary() const
{
    return tfm::format("%3.0f fps", frameRate());
}

std::string
FrameRate::detailed() const
{
    return tfm::format("%5.3f sec %3.0f fps", frameTime(), frameRate());
}

