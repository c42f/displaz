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
#endif


size_t closestPointToRay(const V3f* points, size_t nPoints,
                         const V3f& rayOrigin, const V3f& rayDirection,
                         double longitudinalScale, double* distance)
{
    V3f T = rayDirection.normalized();
    size_t nearestIdx = -1;
    double nearestDist2 = DBL_MAX;
    double f = longitudinalScale*longitudinalScale;
    for(size_t i = 0; i < nPoints; ++i, ++points)
    {
        V3f v = rayOrigin - *points;
        float distN = T.dot(v);
        float distNperp = (v - distN*T).length2();
        double d = f*distN*distN + distNperp;
        if(d < nearestDist2)
        {
            nearestDist2 = d;
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

