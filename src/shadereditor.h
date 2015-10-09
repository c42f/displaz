// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef DISPLAZ_SHADER_EDITOR_H_INCLUDED
#define DISPLAZ_SHADER_EDITOR_H_INCLUDED

#include <QPlainTextEdit>

/// Very basic shader editor widget
class ShaderEditor : public QPlainTextEdit
{
    Q_OBJECT

    public:
        ShaderEditor(QWidget* parent = 0);

        /// Get the shader Compile action
        ///
        /// This action may be triggered from the editor context menu or by the
        /// keyboard shortcut.
        QAction* compileAction() { return m_compileShaderAct; }

    protected:
        void keyPressEvent(QKeyEvent *event);
        void contextMenuEvent(QContextMenuEvent *event);

    private:
        QAction* m_compileShaderAct;
};

#endif // DISPLAZ_SHADER_EDITOR_H_INCLUDED
