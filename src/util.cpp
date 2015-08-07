// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt


#include "util.h"

#ifdef _WIN32
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#   include <io.h>
#else
#   include <unistd.h>
#   include <signal.h>
#endif

#include "tinyformat.h"

size_t closestPointToRay(const V3f* points, size_t nPoints,
                         const V3f& rayOrigin, const V3f& rayDirection,
                         double longitudinalScale, double* distance)
{
    const V3f T = rayDirection.normalized();
    const double f = longitudinalScale*longitudinalScale - 1;
    size_t nearestIdx = -1;
    double nearestDist2 = DBL_MAX;
    for(size_t i = 0; i < nPoints; ++i)
    {
        const V3f v = points[i] - rayOrigin; // vector from ray origin to point
        const double a = v.dot(T); // distance along ray to point of closest approach to test point
        const double r2 = v.length2() + f*a*a;

        if(r2 < nearestDist2)
        {
            // new closest angle to axis
            nearestDist2 = r2;
            nearestIdx = i;
        }
    }
    if(distance)
    {
        if(nPoints == 0)
            *distance = DBL_MAX;
        else
            *distance = sqrt(nearestDist2);
    }
    return nearestIdx;
}


void milliSleep(int msecs)
{
#ifdef _WIN32
    Sleep((DWORD)msecs);
#else
    struct timespec ts = {msecs/1000, (msecs % 1000)*1000*1000};
    nanosleep(&ts, NULL);
#endif
}


std::string currentUserUid()
{
#   ifdef _WIN32
    DWORD sessId = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &sessId);
    return tfm::format("%d", sessId);
#   else
    return tfm::format("%d", ::getuid());
#   endif
}


#ifdef _WIN32

class SigIntTransferHandler::Impl
{
    public:
        // TODO
        Impl(int64_t targetProcess) {}
};

#else

class SigIntTransferHandler::Impl
{
    public:
        Impl(int64_t targetProcess)
            : m_targetProcess(targetProcess)
        {
            assert(!g_impl);
            g_impl = this;
            signal(SIGINT, &passSignalToTarget);
        }

        ~Impl()
        {
            signal(SIGINT, SIG_DFL);
            g_impl = NULL;
        }

    private:
        static void passSignalToTarget(int signum)
        {
            assert(g_impl);
            // Send signal to target and reraise
            kill(g_impl->m_targetProcess, signum);
            signal(SIGINT, SIG_DFL);
            raise(SIGINT);
        }

        int64_t m_targetProcess;

        static Impl* g_impl;
};

SigIntTransferHandler::Impl* SigIntTransferHandler::Impl::g_impl = NULL;

#endif


SigIntTransferHandler::SigIntTransferHandler(int64_t targetProcess)
    : m_impl(new Impl(targetProcess))
{ }

SigIntTransferHandler::~SigIntTransferHandler()
{ }


//------------------------------------------------------------------------------
bool iequals(const std::string& a, const std::string& b)
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (tolower(a[i]) != tolower(b[i]))
            return false;
    return true;
}


bool endswith(const std::string& str, const std::string& suffix)
{
    return str.rfind(suffix) == str.size() - suffix.size();
}

