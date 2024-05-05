// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "Enable.h"

#include <QString>
#include <QStringList>

#include <iostream>

static void glEnableDisable(const std::optional<GLboolean>& enable, GLenum f)
{
    if (enable == GL_TRUE)
    {
        glEnable(f);
        return;
    }

    if (enable == GL_FALSE)
    {
        glDisable(f);
        return;
    }
}

void Enable::enableOrDisable() const
{
    glEnableDisable(DEPTH_TEST,                GL_DEPTH_TEST);
    glEnableDisable(STENCIL_TEST,              GL_STENCIL_TEST);
    glEnableDisable(BLEND,                     GL_BLEND);
    glEnableDisable(VERTEX_PROGRAM_POINT_SIZE, GL_VERTEX_PROGRAM_POINT_SIZE);
}

bool Enable::set(const QString& src)
{
    DEPTH_TEST                = true;
    STENCIL_TEST              = false;
    BLEND                     = false;
    VERTEX_PROGRAM_POINT_SIZE = true;

    // Search source code looking for glEnable/glDisable
    QStringList lines = src.split('\n');
    QRegExp re("// gl(Enable|Disable)\\(([A-Z0-9_]+)\\)", Qt::CaseSensitive, QRegExp::RegExp2);
    for (const auto& line : lines)
    {
        if (!re.exactMatch(line))
            continue;

        const QByteArray enable = re.cap(1).toUtf8().constData();
        const QByteArray name   = re.cap(2).toUtf8().constData();

        const bool e = enable == "Enable";

        std::cout << name.data() << " " << enable.data() << std::endl;

        if (name == "GL_DEPTH_TEST")
        {
            DEPTH_TEST = (e ? GL_TRUE : GL_FALSE);
        }

        if (name == "GL_STENCIL_TEST")
        {
            STENCIL_TEST = (e ? GL_TRUE : GL_FALSE);
        }

        if (name == "GL_BLEND")
        {
            BLEND = (e ? GL_TRUE : GL_FALSE);
        }

        if (name == "GL_VERTEX_PROGRAM_POINT_SIZE")
        {
            VERTEX_PROGRAM_POINT_SIZE = (e ? GL_TRUE : GL_FALSE);
        }
    }

    return true;
}
