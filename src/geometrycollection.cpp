// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "geometrycollection.h"

#include <QtCore/QThread>

#include "tinyformat.h"
#include "fileloader.h"

void GeometryCollection::clear()
{
    emit beginRemoveRows(QModelIndex(), 0, (int)m_geometries.size()-1);
    m_geometries.clear();
    emit endRemoveRows();
}


int GeometryCollection::rowCount(const QModelIndex & parent) const
{
    if (parent.isValid())
        return 0;
    return (int)m_geometries.size();
}


QVariant GeometryCollection::data(const QModelIndex & index, int role) const
{
    if (!index.isValid())
        return QVariant();
    if (role == Qt::DisplayRole)
        return m_geometries[index.row()]->fileName();
    return QVariant();
}


Qt::ItemFlags GeometryCollection::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return 0;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}


bool GeometryCollection::setData(const QModelIndex & index, const QVariant & value, int role)
{
    return false;
}


bool GeometryCollection::removeRows(int row, int count, const QModelIndex& parent)
{
    if (parent.isValid())
        return false;
    if (row < 0 || row+count > (int)m_geometries.size())
        return false;
    emit beginRemoveRows(QModelIndex(), row, row+count-1);
    m_geometries.erase(m_geometries.begin() + row,
                       m_geometries.begin() + row + count);
    emit endRemoveRows();
    return true;
}


void GeometryCollection::setMaxPointCount(size_t maxPointCount)
{
    m_maxPointCount = maxPointCount;
}


void GeometryCollection::loadFiles(const QStringList& fileNames, bool rmTemp)
{
    loadPointFilesImpl(fileNames, rmTemp);
}


void GeometryCollection::reloadFiles()
{
    QStringList fileNames;
    // FIXME: Call reload() on geometries
    for(size_t i = 0; i < m_geometries.size(); ++i)
        fileNames << m_geometries[i]->fileName();
    clear();
    loadPointFilesImpl(fileNames, false);
}


void GeometryCollection::addGeometry(std::shared_ptr<Geometry> geom)
{
    emit beginInsertRows(QModelIndex(), (int)m_geometries.size(),
                         (int)m_geometries.size());
    m_geometries.push_back(geom);
    emit endInsertRows();
}


void GeometryCollection::loadPointFilesImpl(const QStringList& fileNames, bool removeAfterLoad)
{
    emit fileLoadStarted();
    size_t maxCount = fileNames.empty() ? 0 : m_maxPointCount / fileNames.size();

    // Some subtleties regarding qt thread are discussed here
    // http://mayaposch.wordpress.com/2011/11/01/how-to-really-truly-use-qthreads-the-full-explanation
    //
    // Main point: each QObject has a thread affinity which determines which
    // thread its slots will execute on, when called via a connected signal.
    QThread* thread = new QThread;
    FileLoader* loader = new FileLoader(fileNames, maxCount, removeAfterLoad);
    loader->moveToThread(thread);
    //connect(loader, SIGNAL(error(QString)), this, SLOT(errorString(QString)));
    connect(thread, SIGNAL(started()), loader, SLOT(run()));
    connect(loader, SIGNAL(finished()), thread, SLOT(quit()));
    connect(loader, SIGNAL(finished()), loader, SLOT(deleteLater()));
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
    connect(loader, SIGNAL(finished()), this, SIGNAL(fileLoadFinished()));
    connect(loader, SIGNAL(loadStepStarted(QString)),
            this, SIGNAL(loadStepStarted(QString)));
    connect(loader, SIGNAL(loadProgress(int)),
            this, SIGNAL(loadProgress(int)));
    connect(loader, SIGNAL(geometryLoaded(std::shared_ptr<Geometry>)),
            this, SLOT(addGeometry(std::shared_ptr<Geometry>)));
    thread->start();
}


