#version 130

uniform float markerWidth = 0.3;
uniform int selector = 0;
uniform int fileNumber = 0;
uniform int markerShape = 0;

flat in float pointScreenSize;
flat in vec4 pointColor;

out vec4 fragColor;

// Limit at which the point is rendered as a small square for antialiasing
// rather than using a specific marker shape
const float pointScreenSizeLimit = 2;
const float sqrt2 = 1.414213562;

void main()
{
    if (markerShape < 0) // markerShape == -1: discarded.
        discard;
    // (markerShape == 0: Square shape)
    if (markerShape > 0 && pointScreenSize > pointScreenSizeLimit)
    {
        float w = markerWidth;
        if (pointScreenSize < 2*pointScreenSizeLimit)
        {
            // smoothly turn on the markers as we get close enough to see them
            w = mix(1, w, pointScreenSize/pointScreenSizeLimit - 1);
        }
        vec2 p = 2*(gl_PointCoord - 0.5);
        if (markerShape == 1) // shape: .
        {
            float r = length(p);
            if (r > w)
                discard;
        }
        else if (markerShape == 2) // shape: o
        {
            float r = length(p);
            if (r > 1 || r < 1 - w)
                discard;
        }
        else if (markerShape == 3) // shape: x
        {
            w *= 0.5*sqrt2;
            if (abs(p.x + p.y) > w && abs(p.x - p.y) > w)
                discard;
        }
        else if (markerShape == 4) // shape: +
        {
            w *= 0.5;
            if (abs(p.x) > w && abs(p.y) > w)
                discard;
        }
    }
    fragColor = pointColor;
}
