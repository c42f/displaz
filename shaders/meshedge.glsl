#version 150
// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform mat4 modelViewProjectionMatrix;

#if defined(VERTEX_SHADER)
uniform float zoffset = 1e-3;

in vec3 position;
in vec3 color;

out vec4 col;

void main()
{
    gl_Position = modelViewProjectionMatrix * vec4(position,1.0);
    gl_Position.z -= zoffset;
    col = vec4(color, 1.0);
}


//------------------------------------------------------------------------------
#elif defined(FRAGMENT_SHADER)
in vec4 col;

out vec4 fragColor;

void main()
{
    fragColor = col;
}
#endif
