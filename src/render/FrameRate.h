// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

// Adapted from GLT OpenGL Toolkit and re-licensed by author

#pragma once

#include <vector>
#include <string>

#include <QElapsedTimer>

class FrameRate
{
public:
    FrameRate();
    ~FrameRate() = default;

    /// Call this once for each redraw
    FrameRate &operator++();

    /// Get the total elapsed time since construction
    double elapsedTime() const;
    /// Get the total number of frames
    size_t totalFrames() const;

    /// Get the (averaged) frame rate
    double frameRate() const;
    /// Get the (averaged) time per frame
    double frameTime() const;

    /// Summary frame rate information
    std::string summary() const;

    /// Detailed fram rate information
    std::string detailed() const;

private:

    QElapsedTimer        m_timer;               // Timer used for frame rate calculation
    size_t               m_frames = 0;          // Total number of frames
    int64_t              m_step   = 500;        // Minimum time step between recalculation

    int64_t              m_lastCalc = 0;        // Time of last recalculation
    double               m_lastFrameRate = 0.0; // Previously calculated frame rate (frame/s)
    double               m_lastFrameTime = 0.0; // Previously calculated frame time (s/frame)

    std::vector<int64_t> m_buffer;          // storage
};
