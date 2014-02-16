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

#ifndef DISPLAZ_POINTFIELD_H_INCLUDED
#define DISPLAZ_POINTFIELD_H_INCLUDED

#include <cassert>
#include <string>
#include <iostream>
#include <memory>

/// Type description for data fields stored on points
///
/// Each point field is a fixed-length array of data elements made up from
/// simple numeric types.  For example, we might have
///
///   "time"      - double[1]
///   "position"  - float[3]
///   "color"     - uint16_t[3]
///
/// This kind of class comes up in pretty much any rendering API.  For example,
/// OpenImageIO's TypeDesc (descended from gelato).  See also commonalities
/// with PCL's PCD header data.
struct PointFieldType
{
    enum Type
    {
        Float,
        Int,
        Uint,
        Unknown
    };
    Type type;  // Element type
    int elsize; // Element size in bytes
    int count;  // Number of elements

    PointFieldType() : type(Unknown), elsize(0), count(0) {}

    PointFieldType(Type type, int elsize, int count = 1)
        : type(type), elsize(elsize), count(count) {}

    /// Named constructors for common types
    static PointFieldType vec3float32() { return PointFieldType(Float, 4, 3); }
    static PointFieldType float32() { return PointFieldType(Float, 4, 1); }
    static PointFieldType uint16()  { return PointFieldType(Uint, 2, 1); }
    static PointFieldType uint8()   { return PointFieldType(Uint, 1, 1); }

    /// Get number of bytes required to store the field for a single point
    int size() const { return elsize*count; }
};



/// Get associated OpenGL base type for given point field type
///
/// Useful for passing point fields to OpenGL shaders
int glBaseType(const PointFieldType& ftype);


/// Print readable representation of type on stream
std::ostream& operator<<(std::ostream& out, const PointFieldType& ftype);


//------------------------------------------------------------------------------
/// Storage array for point data field
///
/// The data is stored as a packed contiguous array of the base type, with each
/// point having type.count elements.  The name of the field is also included.
/// Here are some standard names used in displaz:
///
///   "position"        - location in 3D space
///   "color"           - RGB color data
///   "returnNumber"    - Return index (1-based) in pulse
///   "numberOfReturns" - Total number of returns in pulse
///   "pointSourceId"   - Identifier for source of aquisition
///   "classification"  - Object type and other data
///
struct PointFieldData
{
    PointFieldType type;          /// Field type
    std::string name;             /// Name of the field
    std::unique_ptr<char[]> data; /// Storage array for values in the point field

    PointFieldData(const PointFieldType& type, const std::string& name, size_t npoints)
        : type(type),
        name(name),
        data(new char[npoints*type.size()])
    { }

    /// Get pointer to the underlying data as array of the base type
    template<typename T>
    T* as()
    {
        assert(sizeof(T) == type.elsize);
        return reinterpret_cast<T*>(data.get());
    }

    /// Reorder the data according to the given indexing array
    void reorder(const size_t* inds, size_t npoints);
};


std::ostream& operator<<(std::ostream& out, const PointFieldData& ftype);


#endif // DISPLAZ_POINTFIELD_H_INCLUDED
