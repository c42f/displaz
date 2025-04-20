// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "DataSetUI.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>

#include <algorithm>

#include "DataSetListView.h"

namespace {

void reduceMinSize(QPushButton* button)
{
    QSize size = button->minimumSize();
    size.setWidth(10);
    button->setMinimumSize(size);
}

}

DataSetUI::DataSetUI(QWidget* parent)
    : QWidget(parent)
{
    // Main view is a list of data sets by name
    m_listView = new DataSetListView(this);
    m_listView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    // Some UI for manipulating the current selection more conveniently than is
    // available by default:
    QWidget* buttons = new QWidget(this);
    QLabel* label = new QLabel(tr("Select:"), buttons);
    QPushButton* selAllButton    = new QPushButton(tr("All"),buttons);
    QPushButton* selNoneButton   = new QPushButton(tr("None"),buttons);
    QPushButton* selInvertButton = new QPushButton(tr("Invert"),buttons);
    // Reduce min size so that buttons don't imply a large minimum width for
    // the whole data set UI
    reduceMinSize(selAllButton);
    reduceMinSize(selNoneButton);
    reduceMinSize(selInvertButton);
    connect(selAllButton,    SIGNAL(clicked()), this, SLOT(selectAll()));
    connect(selNoneButton,   SIGNAL(clicked()), this, SLOT(selectNone()));
    connect(selInvertButton, SIGNAL(clicked()), this, SLOT(selectionInvert()));
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

