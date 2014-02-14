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

#include "datasetui.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>


//------------------------------------------------------------------------------
// DataSetUI implementation

DataSetUI::DataSetUI(QWidget* parent)
    : QWidget(parent),
    m_listView(0)
{
    // Main view is a list of data sets by name
    m_listView = new DataSetListView(this);
    m_listView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    // Some UI for manipulating the current selection more conveniently than is
    // available by default:
    QLabel* label = new QLabel(tr("Select:"), this);
    QPushButton* selAllButton = new QPushButton(tr("All"),this);
    QPushButton* selNoneButton = new QPushButton(tr("None"),this);
    QPushButton* selInvertButton = new QPushButton(tr("Invert"),this);
    connect(selAllButton, SIGNAL(clicked()),
            this, SLOT(selectAll()));
    connect(selNoneButton, SIGNAL(clicked()),
            this, SLOT(selectNone()));
    connect(selInvertButton, SIGNAL(clicked()),
            this, SLOT(selectionInvert()));
    QWidget* buttons = new QWidget(this);
    QHBoxLayout* buttonLayout = new QHBoxLayout(buttons);
    buttonLayout->addWidget(label);
    buttonLayout->addWidget(selAllButton);
    buttonLayout->addWidget(selNoneButton);
    buttonLayout->addWidget(selInvertButton);
    buttonLayout->setContentsMargins(2,2,2,2);
    // Add to overall layout
    QVBoxLayout* dataSetUILayout = new QVBoxLayout(this);
    //dataSetUILayout->setContentsMargins(2,2,2,2);
    dataSetUILayout->addWidget(m_listView);
    dataSetUILayout->addWidget(buttons);
}


QAbstractItemView* DataSetUI::view()
{
    return m_listView;
}


void DataSetUI::selectAll()
{
    const QAbstractItemModel* model = m_listView->model();
    m_listView->selectionModel()->select(
        QItemSelection(model->index(0,0),
                        model->index(model->rowCount()-1,0)),
        QItemSelectionModel::Select
    );
}


void DataSetUI::selectNone()
{
    m_listView->selectionModel()->clearSelection();
}


void DataSetUI::selectionInvert()
{
    QAbstractItemModel* model = m_listView->model();
    m_listView->selectionModel()->select(
        QItemSelection(model->index(0,0),
                        model->index(model->rowCount()-1,0)),
        QItemSelectionModel::Toggle
    );
}


//------------------------------------------------------------------------------
// DataSetListView implementation

void DataSetListView::keyPressEvent(QKeyEvent* event)
{
    if(event->key() == Qt::Key_Delete)
    {
        QModelIndexList sel = selectionModel()->selectedRows();
        for (int i = sel.size()-1; i >= 0; --i)
            model()->removeRows(sel[i].row(), 1);
    }
    else
    {
        QListView::keyPressEvent(event);
    }
}


void DataSetListView::wheelEvent(QWheelEvent* event)
{
    int row = 0;
    if (selectionModel()->currentIndex().isValid())
        row = selectionModel()->currentIndex().row();
    row += (event->delta() < 0) ? 1 : -1;
    if (row < 0)
        row = 0;
    if (row >= model()->rowCount())
        row = model()->rowCount() - 1;
    QModelIndex newIndex = model()->index(row, 0);
    selectionModel()->select(newIndex, QItemSelectionModel::ClearAndSelect);
    selectionModel()->setCurrentIndex(newIndex, QItemSelectionModel::Current);
}


