// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef DISPLAZ_GEOMETRYCOLLECTION_H_INCLUDED
#define DISPLAZ_GEOMETRYCOLLECTION_H_INCLUDED

#include <vector>

#include <QAbstractListModel>
#include <QItemSelectionModel>

#include "geometry.h"


/// Collection of loaded data sets for use with Qt's model view architecture
///
/// Data sets can be points, lines or meshes.
class GeometryCollection : public QAbstractListModel
{
    Q_OBJECT
    public:
        typedef std::vector<std::shared_ptr<Geometry>> GeometryVec;

        GeometryCollection(QObject * parent = 0)
            : QAbstractListModel(parent),
            m_maxPointCount(100000000)
        { }

        /// Get current list of geometries
        const GeometryVec& get() const { return m_geometries; }

        /// Remove all geometries from the list
        void clear();

        // Following implemented from QAbstractListModel:
        virtual int rowCount(const QModelIndex & parent = QModelIndex()) const;
        virtual QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;
        virtual Qt::ItemFlags flags(const QModelIndex& index) const;
        virtual bool setData(const QModelIndex & index, const QVariant & value,
                             int role = Qt::EditRole);

        virtual bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex());


    public slots:
        /// Set maximum total desired number of points
        ///
        /// An attempt will be made to keep the total number of vertices less
        /// than this, but this is quite hard to ensure, so the limit may be
        /// violated.
        void setMaxPointCount(size_t maxPointCount);

        /// Append given file list to the current set of geometries
        ///
        /// If removeAfterLoad is true, *delete* the files after loading.
        void loadFiles(const QStringList& fileNames, bool removeAfterLoad = false);

        /// Reload all currently loaded files from disk
        void reloadFiles();

    signals:
        /// Emitted when files are about to be loaded
        void fileLoadStarted();
        /// Emitted when files are finished loading
        void fileLoadFinished();
        /// Emitted at the start of a point loading step
        void loadStepStarted(QString stepDescription);
        /// Emitted when progress is made loading a file
        void loadProgress(int progress);

    private slots:
        void addGeometry(std::shared_ptr<Geometry> geom);

    private:
        void loadPointFilesImpl(const QStringList& fileNames, bool removeAfterLoad);

        /// Maximum desired number of points to load
        size_t m_maxPointCount;
        GeometryVec m_geometries;
};


#endif // DISPLAZ_GEOMETRYCOLLECTION_H_INCLUDED
