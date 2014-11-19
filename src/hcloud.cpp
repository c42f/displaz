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

#include "hcloud.h"

#include <sstream>
#include <stdexcept>

#include "tinyformat.h"
#include "util.h"

void HCloudHeader::write(std::ostream& out) const
{
    // Construct header in memory buffer so we can easily get the size and seek
    // as required.
    std::stringstream headerBytes;
    headerBytes.write(HCLOUD_MAGIC, HCLOUD_MAGIC_SIZE);
    writeLE<uint16_t>(headerBytes, version);
    std::streampos headerSizePos = headerBytes.tellp();
    writeLE<uint32_t>(headerBytes, headerSize);
    writeLE<uint64_t>(headerBytes, numPoints);
    writeLE<uint64_t>(headerBytes, numVoxels);
    writeLE<uint64_t>(headerBytes, indexOffset);
    writeLE<uint64_t>(headerBytes, dataOffset);
    writeLE<double>(headerBytes, offset.x);
    writeLE<double>(headerBytes, offset.y);
    writeLE<double>(headerBytes, offset.z);
    writeLE<double>(headerBytes, boundingBox.min.x);
    writeLE<double>(headerBytes, boundingBox.min.y);
    writeLE<double>(headerBytes, boundingBox.min.z);
    writeLE<double>(headerBytes, boundingBox.max.x);
    writeLE<double>(headerBytes, boundingBox.max.y);
    writeLE<double>(headerBytes, boundingBox.max.z);
    writeLE<double>(headerBytes, treeBoundingBox.min.x);
    writeLE<double>(headerBytes, treeBoundingBox.min.y);
    writeLE<double>(headerBytes, treeBoundingBox.min.z);
    writeLE<double>(headerBytes, treeBoundingBox.max.x);
    writeLE<double>(headerBytes, treeBoundingBox.max.y);
    writeLE<double>(headerBytes, treeBoundingBox.max.z);
    writeLE<uint16_t>(headerBytes, brickSize);
    headerSize = (uint32_t)headerBytes.tellp();
    headerBytes.seekp(headerSizePos);
    writeLE<uint32_t>(headerBytes, headerSize);
    out << headerBytes.rdbuf();
}

void HCloudHeader::read(std::istream& in)
{
    char magic[HCLOUD_MAGIC_SIZE] = {0};
    in.read(magic, HCLOUD_MAGIC_SIZE);
    if (in.gcount() != HCLOUD_MAGIC_SIZE ||
        memcmp(magic, HCLOUD_MAGIC, HCLOUD_MAGIC_SIZE) != 0)
    {
        throw std::runtime_error("Bad magic number: not a hierarchical point cloud");
    }
    version = readLE<uint16_t>(in);
    if (version != HCLOUD_VERSION)
        throw std::runtime_error("Unknown hcloud version");
    headerSize = readLE<uint32_t>(in);
    numPoints  = readLE<uint64_t>(in);
    numVoxels  = readLE<uint64_t>(in);
    indexOffset = readLE<uint64_t>(in);
    dataOffset = readLE<uint64_t>(in);
    offset.x   = readLE<double>(in);
    offset.y   = readLE<double>(in);
    offset.z   = readLE<double>(in);
    boundingBox.min.x = readLE<double>(in);
    boundingBox.min.y = readLE<double>(in);
    boundingBox.min.z = readLE<double>(in);
    boundingBox.max.x = readLE<double>(in);
    boundingBox.max.y = readLE<double>(in);
    boundingBox.max.z = readLE<double>(in);
    treeBoundingBox.min.x = readLE<double>(in);
    treeBoundingBox.min.y = readLE<double>(in);
    treeBoundingBox.min.z = readLE<double>(in);
    treeBoundingBox.max.x = readLE<double>(in);
    treeBoundingBox.max.y = readLE<double>(in);
    treeBoundingBox.max.z = readLE<double>(in);
    brickSize = readLE<uint16_t>(in);
}

std::ostream& operator<<(std::ostream& out, const HCloudHeader& h)
{
    tfm::format(out,
        "version = %d\n"
        "headerSize = %d\n"
        "numPoints = %d\n"
        "numVoxels = %d\n"
        "indexOffset = %d\n"
        "dataOffset = %d\n"
        "offset = %.3f\n"
        "boundingBox = [%.3f -- %.3f]\n"
        "treeBoundingBox = [%.3f -- %.3f]\n"
        "brickSize = %d",
        h.version,
        h.headerSize,
        h.numPoints,
        h.numVoxels,
        h.indexOffset,
        h.dataOffset,
        h.offset,
        h.boundingBox.min,
        h.boundingBox.max,
        h.treeBoundingBox.min,
        h.treeBoundingBox.max,
        h.brickSize
    );
    return out;
}

