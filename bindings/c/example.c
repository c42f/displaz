#include "displaz.c"

#include <math.h>

#define N 1000

int main()
{
    double position[3*N];
    float color[3*N];

    // Simple example using conical spiral
    int i;
    for (i = 0; i < N; ++i)
    {
        double t = 100*(double)i/N - 50;
        position[3*i] = t*cos(t);
        position[3*i+1] = t*sin(t);
        position[3*i+2] = t;

        color[3*i] = (double)i/N;
        color[3*i+1] = 0;
        color[3*i+2] = 1-(double)i/N;
    }

    displaz_points(N, position, color, NULL);

    return 0;
}
