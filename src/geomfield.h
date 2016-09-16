// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

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

    /// Get pointer to the underlying data as array of the base spec
    template<typename T>
    const T* as() const
    {
        assert(sizeof(T) == spec.elsize);
        return reinterpret_cast<const T*>(data.get());
    }

    /// Print human readable form of `data[index]` to output stream
    void format(std::ostream& out, size_t index) const;

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
