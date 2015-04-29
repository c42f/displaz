// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include <QListView>
#include <QKeyEvent>
#include <QWheelEvent>

#include <iostream>

/// User interface for overview of loaded data sets
///
/// The interface contains mechanisms for showing which data sets are currently
/// loaded, for manipulating the current selection, etc.
class DataSetUI : public QWidget
{
    Q_OBJECT
    public:
        DataSetUI(QWidget* parent = 0);

        /// Return underlying view which displays loaded data set list
        QAbstractItemView* view();

    private slots:
        // Slots to manipulate model selection
        void selectAll();
        void selectNone();
        void selectionInvert();

    private:
        QListView* m_listView;
};


//------------------------------------------------------------------------------
/// List view for data sets with additional mouse and keyboard controls:
///
/// * Pressing delete removes the currently selected elements
/// * Mouse wheel scrolls the current selection rather than the scroll area
class DataSetListView : public QListView
{
    Q_OBJECT
    public:
        DataSetListView(QWidget* parent = 0)
            : QListView(parent)
        { }

        virtual void keyPressEvent(QKeyEvent* event);
        virtual void wheelEvent(QWheelEvent* event);
};

