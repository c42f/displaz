#version 150
// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

uniform mat4 modelViewProjectionMatrix;

//------------------------------------------------------------------------------
#if defined(VERTEX_SHADER)

// Shader parameters which may be set via the user interface
uniform float pointRadius = 0.1;   //# uiname=Point Radius (m); min=0.001; max=10
uniform float exposure = 1.0;      //# uiname=Exposure; min=0.01; max=10000

// Point size multiplier to get from a width in projected coordinates to the
// number of pixels across as required for gl_PointSize
uniform float pointPixelScale = 0;

// Each point has an intensity and position
in float intensity;
in vec3 position;

// Point color which will be picked up by the fragment shader
flat out vec3 pointColor;

void main()
{
    gl_Position = modelViewProjectionMatrix * vec4(position,1.0);
    gl_PointSize = 2*pointPixelScale*pointRadius/gl_Position.w;
    pointColor = exposure*intensity/400.0 * vec3(1);
}


//------------------------------------------------------------------------------
#elif defined(FRAGMENT_SHADER)

// Input color per point
flat in vec3 pointColor;

// Output fragment color
out vec4 fragColor;

void main()
{
    // Trivial fragment shader copies the colour and makes it opaque
    fragColor = vec4(pointColor, 1);
}

#endif

