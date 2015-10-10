#version 150
// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

// Very simple shader setup for rendering a semitransparent selection sphere.

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform mat4 modelViewProjectionMatrix;

//------------------------------------------------------------------------------
#if defined(VERTEX_SHADER)

uniform vec4 color = vec4(1);

in vec3 position;

out vec4 color2;

void main()
{
    color2 = color;
    gl_Position = modelViewProjectionMatrix * vec4(position,1.0);
}


//------------------------------------------------------------------------------
#elif defined(FRAGMENT_SHADER)

in vec4 color2;

out vec4 fragColor;

void main()
{
    fragColor = color2;
}

#endif

