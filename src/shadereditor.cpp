// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "shadereditor.h"

ShaderEditor::ShaderEditor(QWidget* parent)
    : QPlainTextEdit(parent)
{
    setLineWrapMode(QPlainTextEdit::NoWrap);
    // Use fixed-width font
    QFont font("Courier");
    font.setStyleHint(QFont::TypeWriter);
    setFont(font);
}


void ShaderEditor::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return &&
        event->modifiers() & Qt::ShiftModifier)
    {
        emit sendShader(toPlainText());
    }
    else
        QPlainTextEdit::keyPressEvent(event);
}
