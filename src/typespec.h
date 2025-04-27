// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef DISPLAZ_TYPESPEC_H_INCLUDED
#define DISPLAZ_TYPESPEC_H_INCLUDED

#include <cassert>
#include <string>
#include <iostream>
#include <memory>

/// Type description for data fields stored on geometry
///
/// Each point field is a fixed-length array of data elements made up from
/// simple numeric types.  For example, we might have
///
///   "time"      - double[1]
///   "position"  - float[3]
///   "color"     - uint16_t[3]
///
/// Aggregates of more than one element may have different interpretations -
/// they may be arrays of numbers, vectors, colours, etc.  Such differences are
/// captured by the semantics field, which allows us to properly present the
/// types to the OpenGL shader.
///
/// This kind of class comes up in pretty much any rendering API.  For example,
/// OpenImageIO's TypeDesc (descended from gelato), the Aqsis Ri::TypeSpec /
/// PrimVarToken machinary, etc.
struct TypeSpec
{
    enum Type
    {
        Float,
        Int,
        Uint,
        Unknown
    };

    enum Semantics
    {
        Array,
        Vector,
        // Point, Normal // Required if we supported nonrigid transforms
        Color
    };

    Type type;       /// Element type
    int elsize;      /// Element size in bytes
    int count;       /// Number of elements
    Semantics semantics;  /// Interpretation for aggregates
    bool fixedPoint; /// For Int,Uint: indicates fixed point scaling by
                     /// max value of the underlying integer type

    TypeSpec() : type(Unknown), elsize(0), count(0), semantics(Array), fixedPoint(false) {}

    TypeSpec(Type type, int elsize, int count = 1,
             Semantics semantics = Array, bool fixedPoint = false)
        : type(type),
        elsize(elsize),
        count(count),
        semantics(semantics),
        fixedPoint(type == Float ? false : fixedPoint)
    {}

    /// Named constructors for common types
    static TypeSpec vec3float32() { return TypeSpec(Float, 4, 3, Vector); }
    static TypeSpec float32() { return TypeSpec(Float, 4, 1); }
    // Non-scaled integers
    static TypeSpec uint32_i()  { return TypeSpec(Uint, 4, 1, Array, false); }
    static TypeSpec uint16_i()  { return TypeSpec(Uint, 2, 1, Array, false); }
    static TypeSpec uint8_i()   { return TypeSpec(Uint, 1, 1, Array, false); }
    // Scaled fixed point integers
    static TypeSpec uint32()  { return TypeSpec(Uint, 4, 1, Array, true); }
    static TypeSpec uint16()  { return TypeSpec(Uint, 2, 1, Array, true); }
    static TypeSpec uint8()   { return TypeSpec(Uint, 1, 1, Array, true); }

    /// Return number of vector elements in the aggregate
    int vectorSize() const { return (semantics == Array) ? 1 : count; }
    /// Return number of array elements in the aggregate
    int arraySize()  const { return (semantics == Array) ? count : 1; }
    /// Return true when the type represents a nontrivial array
    bool isArray() const { return (semantics == Array) && (count > 1); }

    /// Get number of bytes required to store the field for a single point
    int size() const { return elsize*count; }
    /// Check that two TypeSpecs are exactly equal
    bool operator==(const TypeSpec& other) const
    {
        return type == other.type && elsize == other.elsize &&
            count == other.count && semantics == other.semantics &&
            fixedPoint == other.fixedPoint;
    }
};



/// Get associated OpenGL base type for given point field type
///
/// Useful for passing point fields to OpenGL shaders
int glBaseType(const TypeSpec& spec);


/// Print readable representation of type on stream
std::ostream& operator<<(std::ostream& out, const TypeSpec& spec);


#endif // DISPLAZ_TYPESPEC_H_INCLUDED
