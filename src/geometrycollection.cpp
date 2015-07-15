// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "geometrycollection.h"

#include "tinyformat.h"
#include "fileloader.h"

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


void GeometryCollection::addGeometry(std::shared_ptr<Geometry> geom,
                                     bool reloaded)
{
    if (reloaded)
    {
        for (size_t i = 0; i < m_geometries.size(); ++i)
        {
            if (m_geometries[i]->fileName() == geom->fileName())
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
