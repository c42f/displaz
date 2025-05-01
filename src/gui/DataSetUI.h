// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include <QWidget>
#include <QListView>

/// User interface for overview of loaded data sets
///
/// The interface contains mechanisms for showing which data sets are currently
/// loaded, for manipulating the current selection, etc.
class DataSetUI : public QWidget
{
    Q_OBJECT
    public:
        DataSetUI(QWidget* parent = nullptr);

        /// Return underlying view which displays loaded data set list
        QAbstractItemView* view();

    public slots:
        void selectIndex(int);

    private slots:
        // Slots to manipulate model selection
        void selectAll();
        void selectNone();
        void selectionInvert();

    private:
        QListView* m_listView = nullptr;
};
