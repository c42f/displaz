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

#ifndef DISPLAZ_POINTARRAY_H_INCLUDED
#define DISPLAZ_POINTARRAY_H_INCLUDED

#include <memory>
#include <vector>

#include <QtCore/QObject>

#include "util.h"

class QGLShaderProgram;

class OctreeNode;

//------------------------------------------------------------------------------
/// Container for points to be displayed in the PointView interface
class PointArray : public QObject
{
    Q_OBJECT

    public:
        PointArray();
        ~PointArray();

        /// Load points from a file
        bool loadPointFile(QString fileName, size_t maxPointCount);

        QString fileName() const { return m_fileName; }

        /// Return the number of points
        size_t size() const { return m_npoints; }
        /// Return true when there are zero points
        bool empty() const { return m_npoints == 0; }

        /// Return point position
        ///
        /// Note that this is stored relative to the offset() of the point
        /// cloud to avoid loss of precision.
        const V3f* P() const { return m_P.get(); }

        V3d absoluteP(size_t idx) const { return V3d(m_P[idx]) + m_offset; }

        /// Return index of "closest" point to the given position
        ///
        /// The distance is the euclidian distance where the normal direction
        /// has been scaled by the amout normalDirectionScale.  Also return the
        /// distance to the nearest point if the input distance parameter is
        /// non-null.
        size_t closestPoint(const V3d& pos, const V3f& N,
                            double longitudinalScale, double* distance = 0) const;

        /// Return the centroid of the position data
        const V3d& centroid() const { return m_centroid; }

        /// Get bounding box
        const Imath::Box3d& boundingBox() const { return m_bbox; }

        /// Get the offset which should be added to P to get absolute position
        V3d offset() const { return m_offset; }

        /// Compute the number of points which would be drawn at quality == 1
        size_t simplifiedSize(const V3d& cameraPos, bool incrementalDraw);

        /// Draw points using given openGL shader program
        ///
        /// Requires that prog is already bound and any necessary uniform
        /// variables have been set.
        ///
        /// quality specifies the desired amount of simplification when
        /// simplify is true.  If simplify is false, all points are drawn.
        ///
        /// Return total number of points actually drawn
        size_t draw(QGLShaderProgram& prog, const V3d& cameraPos,
                    double quality, bool simplify, bool incrementalDraw) const;

        /// Draw a representation of the point hierarchy.
        ///
        /// Probably only useful for debugging.
        void drawTree() const;

    signals:
        /// Emitted at the start of a point loading step
        void loadStepStarted(QString stepDescription);
        /// Emitted as progress is made loading points
        void pointsLoaded(int percentLoaded);

    private:
        friend struct ProgressFunc;

        QString m_fileName;
        size_t m_npoints;
        V3d m_offset;
        Imath::Box3d m_bbox;
        V3d m_centroid;
        std::unique_ptr<OctreeNode> m_rootNode;
        std::unique_ptr<V3f[]> m_P;
        std::unique_ptr<float[]> m_intensity;
        std::unique_ptr<C3f[]> m_color;
        std::unique_ptr<unsigned char[]> m_returnNumber;
        std::unique_ptr<unsigned char[]> m_numberOfReturns;
        std::unique_ptr<unsigned char[]> m_pointSourceId;
        std::unique_ptr<unsigned char[]> m_classification;
};


#endif // DISPLAZ_POINTARRAY_H_INCLUDED
