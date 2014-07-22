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


#ifndef DISPLAZ_HCLOUD_H_INCLUDED
#define DISPLAZ_HCLOUD_H_INCLUDED

#include <stdint.h>

#include <OpenEXR/ImathVec.h>
#include <OpenEXR/ImathBox.h>

#define HCLOUD_MAGIC "HierarchicalPointCloud\n\x0c"
#define HCLOUD_MAGIC_SIZE 24
#define HCLOUD_VERSION 0


struct HCloudHeader
{
    uint16_t version;
    mutable uint32_t headerSize;
    uint64_t numPoints;
    uint64_t numVoxels;
    uint64_t indexSize;
    uint64_t dataSize;

    Imath::V3d offset;
    Imath::Box3d boundingBox;

    HCloudHeader()
        : version(HCLOUD_VERSION),
        headerSize(0),
        numPoints(0),
        numVoxels(0),
        indexSize(0),
        dataSize(0),
        offset(0)
    { }

    /// Write HCloud header to given stream
    void write(std::ostream& out) const;

    /// Read HCloud header from given stream
    void read(std::istream& in);
};

std::ostream& operator<<(std::ostream& out, const HCloudHeader& h);


#endif // DISPLAZ_HCLOUD_H_INCLUDED
