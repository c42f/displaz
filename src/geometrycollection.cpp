// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "geometrycollection.h"

#include "tinyformat.h"
#include "fileloader.h"

#include <QRegExp>

GeometryCollection::GeometryCollection(QObject* parent)
    : QAbstractListModel(parent)
{ }


void GeometryCollection::clear()
{
    if (m_geometries.size())
    {
        emit beginRemoveRows(QModelIndex(), 0, (int)m_geometries.size()-1);
        m_geometries.clear();
        emit endRemoveRows();
    }
}

int GeometryCollection::findMatchingRow(const QRegExp & filenameRegex)
{
    for (unsigned row = 0; row < m_geometries.size(); row++)
    {
        if (filenameRegex.exactMatch(m_geometries[row]->label()))
            return row;
    }
    return -1;
}

void GeometryCollection::unloadFiles(const QRegExp & filenameRegex)
{
    while (true)
    {
        int row = findMatchingRow(filenameRegex);

        if (row == -1)
            break;

        this->removeRows(row, 1, QModelIndex());
    }
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
        return m_geometries[index.row()]->label();
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


void GeometryCollection::addGeometry(std::shared_ptr<Geometry> geom,
                                     bool replaceLabel, bool reloaded)
{
    if (replaceLabel || reloaded)
    {
        // FIXME: Doing it this way seems a bit nasty.  The geometry should
        // probably reload itself (or provide a way to create a new clone of
        // the geometry object containing the required metadata which is then
        // reloaded?)
        for (size_t i = 0; i < m_geometries.size(); ++i)
        {
            if ( (replaceLabel && m_geometries[i]->label()    == geom->label()) ||
                 (reloaded     && m_geometries[i]->fileName() == geom->fileName()) )
            {
                m_geometries[i] = geom;
                QModelIndex idx = createIndex((int)i, 0);
                emit dataChanged(idx, idx);
                return;
            }
        }
    }
    emit beginInsertRows(QModelIndex(), (int)m_geometries.size(),
                         (int)m_geometries.size());
    m_geometries.push_back(geom);
    emit endInsertRows();
}
