// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "GeomField.h"

#include <cstdint>

#include <tinyformat.h>


void GeomField::format(std::ostream& out, size_t index) const
{
    for (int j = 0; j < spec.count; ++j)
    {
        const char* val = data.get() + index*spec.size() + j*spec.elsize;
        switch (spec.type)
        {
            case TypeSpec::Float:
                switch (spec.elsize)
                {
                    case 4:  tfm::format(out, "%.7g", *(float*)val);  break;
                    case 8:  tfm::format(out, "%.16g", *(double*)val); break;
                    default: tfm::format(out, "?"); break;
                }
                break;
            case TypeSpec::Int:
                switch (spec.elsize)
                {
                    case 1:  tfm::format(out, "%d", *(int8_t*)val);  break;
                    case 2:  tfm::format(out, "%d", *(int16_t*)val); break;
                    case 4:  tfm::format(out, "%d", *(int32_t*)val); break;
                    case 8:  tfm::format(out, "%d", *(int64_t*)val); break;
                    default: tfm::format(out, "?"); break;
                }
                break;
            case TypeSpec::Uint:
                switch (spec.elsize)
                {
                    case 1:  tfm::format(out, "%d", *(uint8_t*)val);  break;
                    case 2:  tfm::format(out, "%d", *(uint16_t*)val); break;
                    case 4:  tfm::format(out, "%d", *(uint32_t*)val); break;
                    case 8:  tfm::format(out, "%d", *(uint64_t*)val); break;
                    default: tfm::format(out, "?"); break;
                }
                break;
            default:
                tfm::format(out, "unknown");
        }
        if (j < spec.count - 1)
            tfm::format(out, " ");
    }
}


std::ostream& operator<<(std::ostream& out, const GeomField& field)
{
    tfm::format(out, "%s %s", field.spec, field.name);
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


void reorder(GeomField& field, const size_t* inds, size_t indsSize)
{
    size_t size = field.size;
    if (size == 1)
        return;
    assert(size == indsSize);
    int typeSize = field.spec.size();
    std::unique_ptr<char[]> newData(new char[size*typeSize]);
    const char* prevData = field.data.get();
    // Various options to do the reordering in larger chunks than a single byte at a time.
    switch (typeSize)
    {
        case 1:  doReorder<uint8_t,  1>(newData.get(), prevData, inds, size); break;
        case 2:  doReorder<uint16_t, 1>(newData.get(), prevData, inds, size); break;
        case 3:  doReorder<uint8_t,  3>(newData.get(), prevData, inds, size); break;
        case 4:  doReorder<uint32_t, 1>(newData.get(), prevData, inds, size); break;
        case 6:  doReorder<uint16_t, 3>(newData.get(), prevData, inds, size); break;
        case 8:  doReorder<uint64_t, 1>(newData.get(), prevData, inds, size); break;
        case 12: doReorder<uint32_t, 3>(newData.get(), prevData, inds, size); break;
        default:
            switch (typeSize % 8)
            {
                case 0:
                    doReorder<uint64_t>(newData.get(), prevData, inds, size, typeSize/8);
                    break;
                case 4:
                    doReorder<uint32_t>(newData.get(), prevData, inds, size, typeSize/4);
                    break;
                case 2: case 6:
                    doReorder<uint16_t>(newData.get(), prevData, inds, size, typeSize/2);
                    break;
                default:
                    doReorder<uint8_t>(newData.get(),  prevData, inds, size, typeSize);
                    break;
            }
    }
    field.data.swap(newData);
}


