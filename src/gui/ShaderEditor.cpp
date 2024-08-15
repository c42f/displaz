// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "ShaderEditor.h"

#include <memory>
#include <iostream>

#include <QAction>
#include <QMenu>

ShaderEditor::ShaderEditor(QWidget* parent)
    : QPlainTextEdit(parent)
{
    setLineWrapMode(QPlainTextEdit::NoWrap);
    // Use fixed-width font
    QFont font("Courier");
    font.setStyleHint(QFont::TypeWriter);
    setFont(font);
    m_highlighter = new GLSLHighlighter(document());
    m_compileShaderAct = new QAction(tr("&Compile"), this);
    m_compileShaderAct->setShortcut(Qt::Key_Return | Qt::SHIFT);
    m_compileShaderAct->setToolTip(tr("Compile current shader and use in 3D view"));
    addAction(m_compileShaderAct);
}


void ShaderEditor::contextMenuEvent(QContextMenuEvent *event)
{
    // Add compile action into the menu
    std::unique_ptr<QMenu> menu(createStandardContextMenu());
    menu->addSeparator();
    menu->addAction(m_compileShaderAct);
    menu->exec(event->globalPos());
}


void ShaderEditor::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return &&
        event->modifiers() & Qt::ShiftModifier)
    {
        // Ick: QPlainTextEdit won't allow Shift+return to be intercepted by
        // m_compileShaderAct, so trigger it here instead.
        m_compileShaderAct->trigger();
    }
    else
        QPlainTextEdit::keyPressEvent(event);
}
