// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef ENABLE_H_INCLUDED
#define ENABLE_H_INCLUDED

#include <QString>

#include <optional>

#include <GL/glew.h>

class Enable
{
public:
    std::optional<GLboolean> DEPTH_TEST                { GL_TRUE };
    std::optional<GLboolean> BLEND                     { GL_FALSE };
    std::optional<GLboolean> VERTEX_PROGRAM_POINT_SIZE { GL_TRUE };

    void enableOrDisable() const;

    bool set(const QString& src);
};


#endif // ENABLE_H_INCLUDED
