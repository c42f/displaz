// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include <catch.hpp>

#include "util.h"

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

