#version 150
// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform mat4 modelViewProjectionMatrix;

uniform vec3 color;

//------------------------------------------------------------------------------
#if defined(VERTEX_SHADER)

in vec3 position;

// Point color which will be picked up by the fragment shader
out vec4 lineColor;

void main()
{
    lineColor = vec4(1.0,1.0,1.0,0.25);
    gl_Position = modelViewProjectionMatrix * vec4(position,1.0);
}


//------------------------------------------------------------------------------
#elif defined(FRAGMENT_SHADER)

// Input color (per point)
in vec4 lineColor;

// Output fragment color
out vec4 fragColor;

void main()
{
    // Trivial fragment shader copies the colour and makes it opaque
    fragColor = vec4(lineColor);
}

#endif

