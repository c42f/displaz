// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "typespec.h"
#include "glutil.h"

#include <tinyformat.h>

//------------------------------------------------------------------------------
// TypeSpec functions

int glBaseType(const TypeSpec& spec)
{
    switch (spec.type)
    {
        case TypeSpec::Float:
            if (spec.elsize == 2) return GL_HALF_FLOAT;
            if (spec.elsize == 4) return GL_FLOAT;
            if (spec.elsize == 8) return GL_DOUBLE;
            break;
        case TypeSpec::Int:
            if (spec.elsize == 1) return GL_BYTE;
            if (spec.elsize == 2) return GL_SHORT;
            if (spec.elsize == 4) return GL_INT;
            break;
        case TypeSpec::Uint:
            if (spec.elsize == 1) return GL_UNSIGNED_BYTE;
            if (spec.elsize == 2) return GL_UNSIGNED_SHORT;
            if (spec.elsize == 4) return GL_UNSIGNED_INT;
            break;
        default:
            break;
    }
    assert(0 && "Unable to convert TypeSpec -> GL type");
    return GL_BYTE;
}



std::ostream& operator<<(std::ostream& out, const TypeSpec& spec)
{
    if (spec.type == TypeSpec::Float)
    {
        const char* baseTypeStr = "";
        switch (spec.elsize)
        {
            case 2: baseTypeStr = "half";   break;
            case 4: baseTypeStr = "float";  break;
            case 8: baseTypeStr = "double"; break;
            default: baseTypeStr = "?"; assert(0); break;
        }
        tfm::format(out, "%s[%d]", baseTypeStr, spec.count);
        return out;
    }
    const char* baseTypeStr = "";
    switch (spec.type)
    {
        case TypeSpec::Int:     baseTypeStr = "int";     break;
        case TypeSpec::Uint:    baseTypeStr = "uint";    break;
        case TypeSpec::Unknown: baseTypeStr = "unknown"; break;
        default: assert(0);
    }
    tfm::format(out, "%s%d_t[%d]", baseTypeStr, 8*spec.elsize, spec.count);
    return out;
}


