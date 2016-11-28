// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include <catch.hpp>

#include "util.h"

// gcc 4.6 and 4.7 warns/suggests parentheses around == comparison
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wparentheses"
#endif


TEST_CASE("Polygon normals")
{
    // Oriented triangle
    CHECK(polygonNormal({0,0,0,
                         1,0,0,
                         1,1,0},
                         {0,1,2}) == V3d(0,0,1));
    // Opposite orientation
    CHECK(polygonNormal({0,0,0,
                         1,0,0,
                         1,1,0},
                         {0,2,1}) == V3d(0,0,-1));
    // Large translation
    float T = 1e5;
    CHECK(polygonNormal({T+0,T+0,T+0,
                         T+1,T+0,T+0,
                         T+1,T+1,T+0},
                         {0,1,2}) == V3d(0,0,1));
    // Simple oriented quad
    CHECK(polygonNormal({0,0,0,
                         1,0,0,
                         1,1,0,
                         0,1,0},
                         {0,1,2,3}) == V3d(0,0,1));
    // Non-convex
    CHECK(polygonNormal({0,0,0,
                         0.5,0.5,0,
                         1,0,0,
                         0.5,1,0},
                         {0,1,2,3}) == V3d(0,0,1));
    // Non-planar
    CHECK(polygonNormal({0,0,0,
                         1,0,0.1,
                         1,1,0,
                         0,1,0.1},
                         {0,1,2,3}) == V3d(0,0,1));

    // Singular - Repeated vertices and consecutive parallel edges
    CHECK(polygonNormal({0,0,0,
                         0,0,0,
                         0.5,0,0,
                         1,0,0,
                         1,1,0,
                         1,1,0,
                         0,1,0},
                         {0,1,2,3,4,5,6}) == V3d(0,0,1));
}


int identity(int i) { return i; }

TEST_CASE("Simple test for multi_partition")
{
    // Test multi_partition()
    int v[] = { 1, 1, 1, 0, 1, 2, 0, 0, 3, 3, 3 };
    int N = sizeof(v)/sizeof(v[0]);
    int* endIters[] = {0,0,0,0};
    int M = 4;

    multi_partition(v, v + N, &identity, endIters, M);

    int vExpect[] = { 0, 0, 0, 1, 1, 1, 1, 2, 3, 3, 3 };
    for (int i = 0; i < N; ++i)
        CHECK(v[i] == vExpect[i]);

    int classEndInds[] = { 3, 7, 8, 11 };
    for (int i = 0; i < M; ++i)
        CHECK(classEndInds[i] == endIters[i] - &v[0]);
}


TEST_CASE("Bounding cylinder computation")
{
    Box3d box(V3d(1,-1,-1), V3d(2,1,1));
    double dmin = 0, dmax = 0, radius = 0;
    SECTION("axis == x")
    {
        makeBoundingCylinder(box, V3d(1,0,0), dmin, dmax, radius);
        CHECK(dmin == 1);
        CHECK(dmax == 2);
        CHECK(fabs(radius - sqrt(2)) < 1e-15);
    }

    SECTION("axis == y")
    {
        makeBoundingCylinder(box, V3d(0,1,0), dmin, dmax, radius);
        CHECK(dmin == -1);
        CHECK(dmax == 1);
        CHECK(fabs(radius - sqrt(1.25)) < 1e-15);
    }

    SECTION("axis == z")
    {
        makeBoundingCylinder(box, V3d(0,0,1), dmin, dmax, radius);
        CHECK(dmin == -1);
        CHECK(dmax == 1);
        CHECK(fabs(radius - sqrt(1.25)) < 1e-15);
    }

    SECTION("axis along 1,1,-1")
    {
        V3d axis = V3d(1,1,-1).normalized();
        makeBoundingCylinder(box, axis, dmin, dmax, radius);
        CHECK(fabs(dmin - -0.5773502691896258) < 1e-15);
        CHECK(fabs(dmax -  2.3094010767585034) < 1e-15);
        CHECK(fabs(radius - 1.4719601443879744) < 1e-15);
    }
}


TEST_CASE("Isotropic EllipticalDist bound tests")
{
    EllipticalDist dist(V3d(0), V3d(1,0,0), 1);

    // origin inside box
    CHECK(dist.boundNearest(Box3d(V3d(-1,-1,-1), V3d(1,1,1))) == 0);
    // origin on boundary
    CHECK(dist.boundNearest(Box3d(V3d(0,0,0), V3d(1,1,1))) == 0);

    // origin outside
    // box translated along x axis
    CHECK(dist.boundNearest(Box3d(V3d(10,-1,-1), V3d(20,1,1))) == 10);
    // box translated along y axis
    CHECK(dist.boundNearest(Box3d(V3d(-1,-5,-1), V3d(1,-3,1))) <= 3);
    // box translated equally in y,z
    CHECK(dist.boundNearest(Box3d(V3d(-1,-5,-5), V3d(1,-3,-3))) <= 3*sqrt(2));
    // degenerate box in general position
    CHECK(fabs(dist.boundNearest(Box3d(V3d(1,2,3), V3d(1,2,3))) - sqrt(14)) < 1e-15);
}


TEST_CASE("Anisotropic EllipticalDist bound tests")
{
    // Anisotropic, with factor of 10 reduction along x
    EllipticalDist dist(V3d(0), V3d(1,0,0), 0.1);

    // origin inside box
    CHECK(dist.boundNearest(Box3d(V3d(-1,-1,-1), V3d(1,1,1))) == 0);
    // origin on boundary
    CHECK(dist.boundNearest(Box3d(V3d(0,0,0), V3d(1,1,1))) == 0);

    // origin outside
    // box translated along x axis
    CHECK(dist.boundNearest(Box3d(V3d(10,-1,-1), V3d(20,1,1))) == 1);
    // box translated equally in y,z
    CHECK(dist.boundNearest(Box3d(V3d(-1,-5,-5), V3d(1,-3,-3))) <= 3*sqrt(2));
    // degenerate box in general position
    CHECK(fabs(dist.boundNearest(Box3d(V3d(1,2,3), V3d(1,2,3))) - sqrt(0.1*0.1*1*1 + 2*2 + 3*3)) < 1e-15);
}
