#version 150
// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform mat4 modelViewProjectionMatrix;

#if defined(VERTEX_SHADER)
in vec3 position;
in vec3 color;

out vec4 col;

void main()
{
    gl_Position = modelViewProjectionMatrix * vec4(position,1.0);
    // Use a z offset to push edges in front of faces.  We aim for an absolute
    // offset (some small multiple of FLT_EPSILON) after the perspective divide
    // to allow for varying data scales.  Before the perspective divide this
    // corresponds to a multiple of w.
    gl_Position.z -= 1e-5*gl_Position.w;
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
