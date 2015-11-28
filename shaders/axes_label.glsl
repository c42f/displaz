#version 150
// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform mat4 modelViewProjectionMatrix;

uniform vec3 center;
uniform vec3 offset;

uniform sampler2D texture0;

//------------------------------------------------------------------------------
#if defined(VERTEX_SHADER)

in vec2 position;
in vec2 texCoord;

// Point color which will be picked up by the fragment shader
out vec2 textureCoords;

void main()
{
    textureCoords = texCoord;

    vec3 axes_pos = offset;
    vec4 rot_pos = vec4(position,0.0,0.0) + modelViewMatrix * vec4(axes_pos, 1.0);

    gl_Position = projectionMatrix * vec4(rot_pos.xy + center.xy,0.0,1.0);
}


//------------------------------------------------------------------------------
#elif defined(FRAGMENT_SHADER)

// Input texture coordinates
in vec2 textureCoords;

// Output fragment color
out vec4 fragColor;

void main()
{
    // Trivial fragment shader reads from texture
    vec4 tex = texture(texture0, vec2(textureCoords));
    fragColor = tex;
}

#endif

