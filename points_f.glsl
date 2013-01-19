#version 130

uniform float markerWidth = 0.3;
uniform int selector = 0;
uniform int fileNumber = 0;

flat in float pointScreenSize;
flat in int markerShape;
flat in vec4 pointColor;

out vec4 fragColor;

// Limit at which the point is rendered as a small square for antialiasing
// rather than using a specific marker shape
const float pointScreenSizeLimit = 2;

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
        vec2 p = gl_PointCoord - 0.5;
        if (markerShape == 1) // shape: o
        {
            float r = 2*length(p);
            if (r > 1 || r < 1 - w)
                discard;
        }
        else if (markerShape == 2) // shape: x
        {
            w *= 0.5;
            float p1 = p.x + p.y;
            float p2 = p.x - p.y;
            if (abs(p1) > w && abs(p2) > w)
                discard;
        }
        else if (markerShape == 3) // shape: +
        {
            w *= 0.5;
            if (abs(p.x) > w && abs(p.y) > w)
                discard;
        }
    }
    fragColor = pointColor;
}
