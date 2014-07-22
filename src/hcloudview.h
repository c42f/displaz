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


#ifndef DISPLAZ_HCLOUDVIEW_H_INCLUDED
#define DISPLAZ_HCLOUDVIEW_H_INCLUDED

#include "geometry.h"

#include "hcloud.h"
#include "glutil.h"
#include "util.h"

#include <fstream>

struct HCloudNode;

/// Viewer for hcloud file format
///
/// HCloudView uses incremental loading of the LoD structure to avoid loading
/// the whole thing into memory at once.
class HCloudView : public Geometry
{
    Q_OBJECT
    public:
        HCloudView();

        ~HCloudView();

        virtual bool loadFile(QString fileName, size_t maxVertexCount);

        virtual void initializeGL() const;

        virtual void draw(const TransformState& transState, double quality) const;

        virtual size_t pointCount() const;

        virtual size_t simplifiedPointCount(const V3d& cameraPos,
                                            bool incrementalDraw) const;

        virtual V3d pickVertex(const V3d& rayOrigin, const V3d& rayDirection,
                               double longitudinalScale, double* distance = 0) const;

    private:
        std::unique_ptr<HCloudNode> m_rootNode;
};


#endif // DISPLAZ_HCLOUDVIEW_H_INCLUDED
