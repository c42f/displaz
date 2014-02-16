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

#include "pointfield.h"
#include "glutil.h"

#include <tinyformat.h>

//------------------------------------------------------------------------------
// PointFieldType functions

int glBaseType(const PointFieldType& ftype)
{
    switch (ftype.type)
    {
        case PointFieldType::Float:
            if (ftype.elsize == 2) return GL_HALF_FLOAT;
            if (ftype.elsize == 4) return GL_FLOAT;
            if (ftype.elsize == 8) return GL_DOUBLE;
            break;
        case PointFieldType::Int:
            if (ftype.elsize == 1) return GL_BYTE;
            if (ftype.elsize == 2) return GL_SHORT;
            if (ftype.elsize == 4) return GL_INT;
            break;
        case PointFieldType::Uint:
            if (ftype.elsize == 1) return GL_UNSIGNED_BYTE;
            if (ftype.elsize == 2) return GL_UNSIGNED_SHORT;
            if (ftype.elsize == 4) return GL_UNSIGNED_INT;
            break;
        default:
            break;
    }
    assert(0 && "Unable to convert PointFieldType -> GL type");
    return GL_BYTE;
}



std::ostream& operator<<(std::ostream& out, const PointFieldType& ftype)
{
    if (ftype.type == PointFieldType::Float)
    {
        const char* baseTypeStr = "";
        switch (ftype.elsize)
        {
            case 2: baseTypeStr = "half";   break;
            case 4: baseTypeStr = "float";  break;
            case 8: baseTypeStr = "double"; break;
            default: baseTypeStr = "?"; assert(0); break;
        }
        tfm::format(out, "%s[%d]", baseTypeStr, ftype.count);
        return out;
    }
    const char* baseTypeStr = "";
    switch (ftype.type)
    {
        case PointFieldType::Int:     baseTypeStr = "int";     break;
        case PointFieldType::Uint:    baseTypeStr = "uint";    break;
        case PointFieldType::Unknown: baseTypeStr = "unknown"; break;
        default: assert(0);
    }
    tfm::format(out, "%s%d_t[%d]", baseTypeStr, 8*ftype.elsize, ftype.count);
    return out;
}


//------------------------------------------------------------------------------
// PointFieldData functions

std::ostream& operator<<(std::ostream& out, const PointFieldData& field)
{
    tfm::format(out, "%s %s", field.type, field.name);
    return out;
}


template<typename T, int count>
void doReorder(char* dest, const char* src, const size_t* inds, size_t size)
{
    T* destT = (T*)dest;
    const T* srcT = (const T*)src;
    for (size_t i = 0; i < size; ++i)
    {
        for (int j = 0; j < count; ++j)
            destT[count*i + j] = srcT[count*inds[i] + j];
    }
}

template<typename T>
void doReorder(char* dest, const char* src, const size_t* inds, size_t size, int count)
{
    T* destT = (T*)dest;
    const T* srcT = (const T*)src;
    for (size_t i = 0; i < size; ++i)
    {
        for (int j = 0; j < count; ++j)
            destT[count*i + j] = srcT[count*inds[i] + j];
    }
}

void PointFieldData::reorder(const size_t* inds, size_t size)
{
    int typeSize = type.size();
    std::unique_ptr<char[]> tmpData(new char[size*typeSize]);
    // Various options to do the reordering in larger chunks than a single byte at a time.
    switch (typeSize)
    {
        case 1:  doReorder<uint8_t,  1>(tmpData.get(), data.get(), inds, size); break;
        case 2:  doReorder<uint16_t, 1>(tmpData.get(), data.get(), inds, size); break;
        case 3:  doReorder<uint8_t,  3>(tmpData.get(), data.get(), inds, size); break;
        case 4:  doReorder<uint32_t, 1>(tmpData.get(), data.get(), inds, size); break;
        case 6:  doReorder<uint16_t, 3>(tmpData.get(), data.get(), inds, size); break;
        case 8:  doReorder<uint64_t, 1>(tmpData.get(), data.get(), inds, size); break;
        case 12: doReorder<uint32_t, 3>(tmpData.get(), data.get(), inds, size); break;
        default:
            switch (typeSize % 8)
            {
                case 0:
                    doReorder<uint64_t>(tmpData.get(), data.get(), inds, size, typeSize/8);
                    break;
                case 4:
                    doReorder<uint32_t>(tmpData.get(), data.get(), inds, size, typeSize/4);
                    break;
                case 2: case 6:
                    doReorder<uint16_t>(tmpData.get(), data.get(), inds, size, typeSize/2);
                    break;
                default:
                    doReorder<uint8_t>(tmpData.get(),  data.get(), inds, size, typeSize);
                    break;
            }
    }
    data.swap(tmpData);
}


