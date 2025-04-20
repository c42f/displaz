// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include <QListView>
#include <QKeyEvent>
#include <QWheelEvent>

//------------------------------------------------------------------------------
/// List view for data sets with additional mouse and keyboard controls:
///
/// * Pressing delete removes the currently selected elements
/// * Mouse wheel scrolls the current selection rather than the scroll area
class DataSetListView : public QListView
{
    Q_OBJECT
    public:
        DataSetListView(QWidget* parent = nullptr);

    signals:
        void reloadFile(const QModelIndex& index);

    private slots:
        void customContextMenu(const QPoint &point);

    protected:
        virtual void keyPressEvent(QKeyEvent* event);
        virtual void wheelEvent(QWheelEvent* event);
};

