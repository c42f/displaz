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
/*        V3f v = rayOrigin - *points;
        float distN = T.dot(v);
        float distNperp = (v - distN*T).length2();
        double d = f*distN*distN + distNperp;
        if(d < nearestDist2)
        {
            nearestDist2 = d;
            nearestIdx = i;
        }*/

        const V3f v = points[i] - rayOrigin; // vector from ray origin to point
        const double l2 = v.length2(); // square of hypotenuse length
        const double a = v.dot(T); // distance along ray to point of closest approach to test point
        const double r2 = l2 + f*a*a;

        if(r2 < nearestDist2)
        {
            // new closest angle to axis
            nearestDist2 = r2;
            nearestIdx = i;
        }
    }
    if(distance)
        *distance = sqrt(nearestDist2);
    return nearestIdx;
}


void attachToParentConsole()
{
#ifdef _WIN32
    // The following small but helpful snippet was found in the firefox source
    if (AttachConsole(ATTACH_PARENT_PROCESS))
    {
        // Change std handles to refer to new console handles.
        // Before doing so, ensure that stdout/stderr haven't been
        // redirected to a valid file
        if (_fileno(stdout) == -1 || _get_osfhandle(_fileno(stdout)) == -1)
            freopen("CONOUT$", "w", stdout);
        // Merge stderr into CONOUT$ since there isn't any `CONERR$`.
        // http://msdn.microsoft.com/en-us/library/windows/desktop/ms683231%28v=vs.85%29.aspx
        if (_fileno(stderr) == -1 || _get_osfhandle(_fileno(stderr)) == -1)
            freopen("CONOUT$", "w", stderr);
        if (_fileno(stdin) == -1 || _get_osfhandle(_fileno(stdin)) == -1)
            freopen("CONIN$", "r", stdin);
        std::cout << "\n";
    }
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

