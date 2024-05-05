// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef DISPLAZ_GEOMETRYMUTATOR_H_INCLUDED
#define DISPLAZ_GEOMETRYMUTATOR_H_INCLUDED

#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>

#include <QObject>
#include <QString>
#include <QMetaType>
#include "util.h"
#include "typespec.h"
#include "GeomField.h"


//------------------------------------------------------------------------------
/// Container for loaded data which will be used to modify (mutate) an
/// existing PointArray.
class GeometryMutator : public QObject
{
    Q_OBJECT

    public:
        GeometryMutator();
        ~GeometryMutator();

        bool loadFile(const QString& fileName);

        /// Get the arbitrary user-defined label for the geometry.
        const QString& label() const { return m_label; }

        /// Set the arbitrary user-defined label for the geometry.
        void setLabel(const QString& label) { m_label = label; }

        /// Get number of points to mutate
        size_t pointCount() const { return m_npoints; }

        /// Get index of points to mutate
        uint32_t* index() const { return m_index; }

        /// Return offset applied to "position" field.
        const V3d& offset() const { return m_offset; }

        /// Get fields to mutate
        const std::vector<GeomField>& fields() const { return m_fields; }

    private:

        /// Label of data to mutate
        QString m_label;
        /// Total number of loaded points
        size_t m_npoints;
        /// Position offset
        V3d m_offset;
        /// Point data field storage
        std::vector<GeomField> m_fields;
        /// An index field is required, plus an alias for convenience:
        int m_indexFieldIdx;
        uint32_t* m_index;
};

Q_DECLARE_METATYPE(std::shared_ptr<GeometryMutator>)

#endif // DISPLAZ_GEOMETRYMUTATOR_H_INCLUDED
