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


