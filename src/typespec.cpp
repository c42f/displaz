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


