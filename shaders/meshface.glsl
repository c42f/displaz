#version 150
// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform mat4 modelViewProjectionMatrix;

#if defined(VERTEX_SHADER)

in vec3 position;
in vec3 normal;
in vec3 color;
in vec2 texCoord;
out vec3 position_eye;
out vec3 fcolor;
out vec2 ftexCoord;

void main()
{
    gl_Position = modelViewProjectionMatrix * vec4(position,1.0);
    position_eye = (modelViewMatrix * vec4(position,1.0)).xyz;
    fcolor = color;
    ftexCoord = texCoord;
    //gl_FrontColor = vec4(color*abs(dot(normal, lightDir)), 1.0);
}


//------------------------------------------------------------------------------
#elif defined(FRAGMENT_SHADER)

uniform vec3 lightDir_eye = vec3(0.0,0.0,-1.0);

uniform bool hasTexture = false;
uniform sampler2D texture0;

in vec3 position_eye;
in vec3 fcolor;
in vec2 ftexCoord;

out vec4 fragColor;

void main()
{
    if (hasTexture)
    {
        fragColor = texture(texture0, vec2(ftexCoord));
    }
    else
    {
        // Faceted shading, without requiring special support in the C++ code.
        vec3 normal = normalize(cross(dFdx(position_eye), dFdy(position_eye)));
        fragColor = vec4(fcolor*abs(dot(normal, lightDir_eye)), 1);
    }
}

#endif

