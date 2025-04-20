// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "DataSetListView.h"

#include <QDebug>

DataSetListView::DataSetListView(QWidget* parent)
: QListView(parent)
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(customContextMenu(const QPoint &)));
}

void DataSetListView::keyPressEvent(QKeyEvent* event)
{
    switch (event->key())
    {
        case Qt::Key_Delete:
        {
            QModelIndexList sel = selectionModel()->selectedRows();
            std::sort(sel.begin(), sel.end());
            for (int i = sel.size()-1; i >= 0; --i)
                model()->removeRows(sel[i].row(), 1);
            return;
        }

        case Qt::Key_R:
        {
            QModelIndexList sel = selectionModel()->selectedRows();
            for (const auto& i : sel)
            {
                emit reloadFile(i);
            }
            return;
        }

        default:
        {
            QListView::keyPressEvent(event);
            return;
        }
    }
}

void DataSetListView::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() & Qt::ControlModifier)
    {
        int row = 0;
        if (selectionModel()->currentIndex().isValid())
            row = selectionModel()->currentIndex().row();
        row += (event->angleDelta().y() < 0) ? 1 : -1;
        if (row < 0)
            row = 0;
        if (row >= model()->rowCount())
            row = model()->rowCount() - 1;
        QModelIndex newIndex = model()->index(row, 0);
        selectionModel()->select(newIndex, QItemSelectionModel::ClearAndSelect);
        selectionModel()->setCurrentIndex(newIndex, QItemSelectionModel::Current);
    }
    else
    {
        QListView::wheelEvent(event);
    }
}

void DataSetListView::customContextMenu(const QPoint &point)
{
    QModelIndex index = indexAt(point);

    if (index.isValid())
    {
        qDebug() << index.data();
    }
    else
    {
        qDebug() << "None";
    }
}
