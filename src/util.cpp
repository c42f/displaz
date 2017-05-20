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
#   include <shellapi.h> // For CommandLineToArgvW
#   include <io.h>
#else
#   include <unistd.h>
#   include <signal.h>
#endif

#include "tinyformat.h"

size_t EllipticalDist::findNearest(const V3d& offset, const V3f* points,
                                   size_t nPoints, double* distance) const
{
    V3f offsetOrigin = V3f(m_origin - offset);
    const double f = m_scale*m_scale - 1;
    size_t nearestIdx = -1;
    double nearestDist2 = DBL_MAX;
    for(size_t i = 0; i < nPoints; ++i)
    {
        const V3f v = points[i] - offsetOrigin; // vector from ray origin to point
        const double a = m_axis.dot(v); // distance along ray to point of closest approach to test point
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


double EllipticalDist::boundNearest(const Box3d& box) const
{
    // We want the minimum elliptical distance from the origin to a point in
    // the box.  Unfortunately this is pretty hard to compute in general, but
    // it's quite easy to approximate with a bounding cylinder with axis along
    // the direction m_axis.

    // Offset to be relative to m_origin
    Box3d offsetBox = box;
    offsetBox.min -= m_origin;
    offsetBox.max -= m_origin;

    // Compute bounding cylinder
    double dmin = 0, dmax = 0, radius = 0;
    makeBoundingCylinder(offsetBox, m_axis, dmin, dmax, radius);
    V3d center = offsetBox.center();

    // Compute distance components from origin to cylinder, parallel and
    // perpendicular to axis.
    double parallelDist = 0;
    if (dmin > 0)
        parallelDist = dmin;
    else if (dmax < 0)
        parallelDist = dmax;
    double centerPerpDist = (center - m_axis.dot(center)*m_axis).length();
    double perpDist = std::max(centerPerpDist - radius, 0.0);

    parallelDist *= m_scale; // Add elliptical scale factor
    return sqrt(pow(parallelDist,2) + pow(perpDist,2));
}


void makeBoundingCylinder(const Box3d& box, const V3d& axis,
                          double& dmin, double& dmax, double& radius)
{
    assert(fabs(axis.length() - 1) < 1e-15);

    // List of box vertices. Any point inside is a convex combination of these.
    V3d verts[] = {
        V3d(box.min.x, box.min.y, box.min.z),
        V3d(box.min.x, box.max.y, box.min.z),
        V3d(box.max.x, box.max.y, box.min.z),
        V3d(box.max.x, box.min.y, box.min.z),
        V3d(box.min.x, box.min.y, box.max.z),
        V3d(box.min.x, box.max.y, box.max.z),
        V3d(box.max.x, box.max.y, box.max.z),
        V3d(box.max.x, box.min.y, box.max.z),
    };

    // Compute bounding cylinder with axis through box center, and dmin, dmax,
    // radius relative to box center.
    V3d center = box.center();

    // d = dimension along cylinder
    double halfLength = -DBL_MAX;
    double cradius2 = 0;
    for (int i = 0; i < 8; ++i)
    {
        V3d v = verts[i] - center;
        double d = axis.dot(v);
        halfLength = std::max(halfLength, d);
        cradius2 = std::max(cradius2, v.length2() - d*d);
    }
    radius = sqrt(cradius2);
    double dc = axis.dot(center);
    dmax = dc + halfLength;
    dmin = dc - halfLength;
}


V3d polygonNormal(const std::vector<float>& verts,
                  const std::vector<unsigned int>& outerRingInds)
{
    V3d normal(0,0,0);
    // Do some extra work removing a fixed origin to greatly reduce
    // cancellation error.  Start with the last vertex for convenience looping
    // circularly around all vertex pairs.
    unsigned int j = 3*outerRingInds.back();
    V3d origin = V3d(verts[j], verts[j+1], verts[j+2]);
    V3d prevVert = V3d(0,0,0); // origin-origin
    for (size_t i = 0; i < outerRingInds.size(); ++i)
    {
        unsigned int j = 3*outerRingInds[i];
        assert(j + 2 < verts.size());
        V3d vert = V3d(verts[j], verts[j+1], verts[j+2]) - origin;
        normal += prevVert.cross(vert);
        prevVert = vert;
    }
    return normal.normalized();
}


//------------------------------------------------------------------------------

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
void getDisplazIpcNames(std::string& socketName, std::string& lockFileName,
                        const std::string& serverName)
{
    std::string id = "displaz-ipc-" + currentUserUid();
    if (!serverName.empty())
        id += "-" + serverName;
    socketName = id;
    lockFileName = id + ".lock";
}


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

#ifdef _WIN32
/// Utility for windows: Convert to UTF-8 from UTF-16
static std::string utf8FromUtf16(const std::wstring &wstr)
{
    if (wstr.empty())
        return std::string();
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(),
                                         NULL, 0, NULL, NULL);
    std::string strTo(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(),
                        &strTo[0], sizeNeeded, NULL, NULL);
    return strTo;
}
#endif

void ensureUtf8Argv(int* argc, char*** argv)
{
#   ifdef _WIN32
    static std::vector<std::string> stringStorage;
    static std::vector<char*> argvStorage;
    int argc2 = 0;
    LPWSTR* argvw = CommandLineToArgvW(GetCommandLineW(), &argc2);
    stringStorage.resize(argc2);
    argvStorage.resize(argc2);
    for (int i = 0; i < argc2; ++i)
    {
        stringStorage[i] = utf8FromUtf16(argvw[i]);
        argvStorage[i] = &stringStorage[i][0];
    }
    *argc = argc2;
    *argv = &argvStorage[0];
#   endif
}


