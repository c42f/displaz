// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "displaz.h"
#include <cmath>

// Simple example of using displaz to plot points from a separate proces
// Compile in C++11 mode.
int main()
{
    displaz::Window win;
    win.setDebug(true);

    displaz::PointList points;
    points.addAttribute<double>("position", 3)
          .addAttribute<float>("intensity", 1)
          .addAttribute<uint8_t>("color", 3);


    int N = 10000;
    for (int i = 0; i < N; ++i)
    {
        double t = double(i)/N;
        double r= 10*sqrt(t) + 2;
        points.append(r*cos(200*t), r*sin(200*t), 10*t,
                      255*t,
                      255*t, 255*(1-t), 0);
    }

    points.append(0, 0, 0,
                  1000,
                  0, 0, 255);

    win.setShader("generic_points.glsl");

    win.plot(points);

    return 0;
}
