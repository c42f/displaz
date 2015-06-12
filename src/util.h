// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

#include <OpenEXR/ImathVec.h>
#include <OpenEXR/ImathBox.h>
#include <OpenEXR/ImathColor.h>
#include <OpenEXR/ImathMatrix.h>

#include <iostream>
#include <stdexcept>
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


/// Return index of closest point to the given ray, favouring points near ray
/// origin.
///
/// The distance function is the euclidian distance from the ray origin to the
/// point, where the space has been scaled along the ray direction by the amout
/// longitudinalScale.  Also return the distance to the nearest point if the
/// input distance parameter is non-null.
size_t closestPointToRay(const V3f* points, size_t nPoints,
                         const V3f& rayOrigin, const V3f& rayDirection,
                         double longitudinalScale, double* distance = 0);


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

/// On windows, attach to the console of the parent process if possible.  On
/// other platforms, do nothing.
///
/// By default, stdout and stderr disappear completely for windows GUI
/// programs.  This is rather unhelpful when using or debugging a program from
/// the console, so this function can be used to re-attach to the parent
/// console (if it's present) after startup.  This lets us get console
/// messages, while also avoiding an annoying extra window when started
/// normally via the GUI.
void attachToParentConsole();


/// Get a unique id string for the current user
///
/// On unix this is just the user's numeric id; on windows, it's the session
/// id, which isn't quite the same thing but should work as a unique id on the
/// machine.
std::string currentUserUid();


//------------------------------------------------------------------------------
// String utils

/// Case-insensitive comparison of two strings for equality
bool iequals(const std::string& a, const std::string& b);


/// Return true if string ends with the given suffix
bool endswith(const std::string& str, const std::string& suffix);



#endif // UTIL_H_INCLUDED
