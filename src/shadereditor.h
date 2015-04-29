// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef DISPLAZ_SHADER_EDITOR_H_INCLUDED
#define DISPLAZ_SHADER_EDITOR_H_INCLUDED

#include <QtGui/QPlainTextEdit>

class ShaderEditor : public QPlainTextEdit
{
    Q_OBJECT

    public:
        ShaderEditor(QWidget* parent = 0);

    signals:
        void sendShader(QString src);

    protected:
        void keyPressEvent(QKeyEvent *event);
};

#endif // DISPLAZ_SHADER_EDITOR_H_INCLUDED
