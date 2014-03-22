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

#ifndef DISPLAZ_GEOMFIELD_H_INCLUDED
#define DISPLAZ_GEOMFIELD_H_INCLUDED

#include "typespec.h"


//------------------------------------------------------------------------------
/// Storage array for scalar and vector fields on a geometry
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
struct GeomField
{
    TypeSpec spec;                /// Field type
    std::string name;             /// Name of the field
    std::unique_ptr<char[]> data; /// Storage array for values in the point field
    size_t size;                  /// Number of elements in array

    GeomField(const TypeSpec& spec, const std::string& name, size_t size)
        : spec(spec),
        name(name),
        data(new char[size*spec.size()]),
        size(size)
    { }

    /// Get pointer to the underlying data as array of the base spec
    template<typename T>
    T* as()
    {
        assert(sizeof(T) == spec.elsize);
        return reinterpret_cast<T*>(data.get());
    }

    // Horrible hack: explicitly implement move constructor.  Required to
    // appease MSVC 2012 (broken move semantics for unique_ptr?)
    GeomField(GeomField&& f)
        : spec(f.spec), name(f.name), data(f.data.release()), size(f.size)
    { }
};


/// Reorder point field data according to the given indexing array
void reorder(GeomField& field, const size_t* inds, size_t indsSize);


std::ostream& operator<<(std::ostream& out, const GeomField& field);


#endif // DISPLAZ_GEOMFIELD_H_INCLUDED
