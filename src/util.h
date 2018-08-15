// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

#include <cmath>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

#ifdef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-register"
#endif

#include <Imath/ImathVec.h>
#include <Imath/ImathBox.h>
#include <Imath/ImathColor.h>
#include <Imath/ImathMatrix.h>

#ifdef __clang__
#pragma GCC diagnostic pop
#endif

#include <tinyformat.h>


//------------------------------------------------------------------------------
/// Exception class with typesafe printf-like error formatting in constructor
struct DisplazError : public std::runtime_error
{
    DisplazError(const std::string& err) : std::runtime_error(err) { }

    // template<typename... T>
    // DisplazError(const char* fmt, const T&... vals);
#   define MAKE_CONSTRUCTOR(n)                                          \
    template<TINYFORMAT_ARGTYPES(n)>                                    \
    DisplazError(const char* fmt, TINYFORMAT_VARARGS(n))            \
        : std::runtime_error(tfm::format(fmt, TINYFORMAT_PASSARGS(n)))  \
    { }
    TINYFORMAT_FOREACH_ARGNUM(MAKE_CONSTRUCTOR)
#   undef MAKE_CONSTRUCTOR
};


//------------------------------------------------------------------------------
// Math utils

using Imath::V2f;
using Imath::V3i;
using Imath::V3f;
using Imath::V3d;
using Imath::V4d;
using Imath::M44f;
using Imath::M44d;
using Imath::C3f;
using Imath::Box3f;
using Imath::Box3d;


/// Axially symmetric elliptical distance function
///
/// Distance function equal to the usual Euclidian distance from an origin to a
/// point, but where the space has been scaled along a given axis.
class EllipticalDist
{
    public:
        /// Construct elliptical distance from given `origin` where the
        /// component of any vector along `axis` is scaled by `scale`.
        ///
        /// For example, if axis == [1,0,0] and scale == 0.1, distances are
        /// calculated as sqrt(0.01*x*x + y*y + z*z).
        EllipticalDist(const V3d& origin, const V3d& axis, double scale)
            : m_origin(origin), m_axis(axis.normalized()), m_scale(scale)
        { }

        /// Return index of point nearest to the origin
        ///
        /// The input point set is offset so that the true positions are
        /// (points[i] + offset) for i in [0,nPoints).
        ///
        /// Also return the distance to the nearest point if the input distance
        /// parameter is non-null.
        size_t findNearest(const V3d& offset, const V3f* points,
                           size_t nPoints, double* distance = 0) const;

        /// Return lower bound on elliptical distance to any point in `box`
        double boundNearest(const Box3d& box) const;

        V3d origin()   const { return m_origin; }
        V3d axis()     const { return m_axis; }
        double scale() const { return m_scale; }

    private:
        V3d m_origin;
        V3d m_axis;
        double m_scale;
};


/// Encapsulate `box` in a cylinder with normalized axis vector `axis`.
///
/// The cylinder axis goes through `box.center()`, with the returned axial
/// extent given by the interval `[dmin,dmax]`, where the coordinate d for a
/// vector `x` is given by `d = axis.dot(x)`.  `radius` is the computed radius.
void makeBoundingCylinder(const Box3d& box, const V3d& axis,
                          double& dmin, double& dmax,
                          double& radius);


// Robustly compute a polygon normal with Newell's method
//
// This can handle non-convex, non-planar polygons with arbitrarily many
// consecutive parallel edges.
//
// verts - 3D vertex positions
// outerRingInds  - Indices of outer boundary vertices in `verts` array;
//                  the j'th component of the i'th polygon vertex is
//                  verts[3*outerRingInds[i]+j].
V3d polygonNormal(const std::vector<float>& verts,
                  const std::vector<unsigned int>& outerRingInds);


/// In-place partition of elements into multiple classes.
///
/// multi_partition partitions the range [first,last) into numClasses groups,
/// based on the value of classFunc evaluated on each element.  classFunc(elem)
/// must return an integer in the range [0,numClasses) which specifies the
/// class of the element.
///
/// classEndIters is the start of a random access sequence of length
/// numClasses.  On return, classEndIters[c] will contain an IterT iterator
/// pointing to the end of the range occupied by elements of class c.
///
/// For efficiency, the number of classes should be small compared to the
/// length of the range.  If not, it's much better just to use std::sort, since
/// multi_partition is quite similar to a bubble sort in the limit of a large
/// number of classes.
template<typename IterT, typename IterTIter, typename ClassFuncT>
void multi_partition(IterT first, IterT last, ClassFuncT classFunc,
                     IterTIter classEndIters, int numClasses)
{
    for (int j = 0; j < numClasses; ++j)
        classEndIters[j] = first;
    for (IterT i = first; i != last; ++i)
    {
        int c = classFunc(*i);
        // Perform swaps to bubble the current element down
        for (int j = numClasses-1; j > c; --j)
        {
            std::swap(*classEndIters[j], *classEndIters[j-1]);
            ++classEndIters[j];
        }
        ++classEndIters[c];
    }
}


/// Return true if box b1 contains box b2
template<typename T>
bool contains(const Imath::Box<T> b1, const Imath::Box<T> b2)
{
    for (unsigned int i = 0; i < T::dimensions(); ++i)
        if (b2.min[i] < b1.min[i] || b2.max[i] > b1.max[i])
            return false;
    return true;
}


/// Print bounding box to stream
template<typename T>
std::ostream& operator<<(std::ostream& out, const Imath::Box<T>& b)
{
    out << b.min << "--" << b.max;
    return out;
}


/// Tile coordinate for las tiling
typedef Imath::Vec3<int> TilePos;

/// Simple lexicographic tile ordering for use with std::map
struct TilePosLess
{
    inline bool operator()(const TilePos& p1, const TilePos& p2) const
    {
        if (p1.x != p2.x)
            return p1.x < p2.x;
        if (p1.y != p2.y)
            return p1.y < p2.y;
        return p1.z < p2.z;
    }
};


//------------------------------------------------------------------------------
// Resource Acquisition Is Initialization (RAII) utility for FILE pointer
class File
{
public:
    inline File(FILE* f = NULL) : file(f) {}
    inline ~File()                        { if (file) fclose(file); file = NULL; }
    inline File & operator=(FILE* f)      { if (file) fclose(file); file = f; return *this; }
    inline operator FILE* ()              { return file; }

private:
    FILE* file;
};


//------------------------------------------------------------------------------
// Binary IO utils

/// Write POD type to std::ostream in little endian binary format
template<typename T>
void writeLE(std::ostream& out, const T& val)
{
    out.write((const char*)&val, sizeof(val));
}

/// Read POD type from std::istream in little endian binary format
template<typename T>
T readLE(std::istream& in)
{
    T val;
    in.read((char*)&val, sizeof(val));
    if (!in || in.gcount() != sizeof(val))
        throw DisplazError("Could not read from stream");
    return val;
}


//------------------------------------------------------------------------------
// System utils

/// Get a unique id string for the current user
///
/// On unix this is just the user's numeric id; on windows, it's the session
/// id, which isn't quite the same thing but should work as a unique id on the
/// machine.
std::string currentUserUid();


/// Sleep for msecs milliseconds
void milliSleep(int msecs);


/// Utility to transfer SIGINT (as generated by command line ^C interrupt) to a
/// given target process, and re-raise in the current process.
///
/// Since only a single signal handler may be active at a time, only a single
/// one of these objects may be instantiated at a time.
///
/// NOTE: Currently not implemented on windows.
class SigIntTransferHandler
{
    public:
        SigIntTransferHandler(int64_t targetProcess);
        ~SigIntTransferHandler();

    private:
        class Impl; // System-dependent implementation
        std::unique_ptr<Impl> m_impl;
};


/// Get socket and lock file names for displaz IPC
///
/// This is a combination of the program name and user name to avoid any name
/// clashes, along with a user-defined serverName.
void getDisplazIpcNames(std::string& socketName, std::string& lockFileName,
                        const std::string& serverName);


//------------------------------------------------------------------------------
// String utils

/// Case-insensitive comparison of two strings for equality
bool iequals(const std::string& a, const std::string& b);


/// Return true if string ends with the given suffix
bool endswith(const std::string& str, const std::string& suffix);

/// Get the argument string array to main() in UTF-8 encoding.
///
/// On windows, this function looks up the true unicode command line argument
/// list via GetCommandLineW() and re-encodes it as UTF-8, overriding the
/// input.  On other platforms, this function does nothing (ie, we assume the
/// input is already encoded in UTF-8.
void ensureUtf8Argv(int* argc, char*** argv);

#endif // UTIL_H_INCLUDED
