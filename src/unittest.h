// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

// Trivial unit test utilities

#include <cstdlib>
#include "tinyformat.h"

#define CHECK_EQUAL(a, b)                                      \
if(!((a) == (b)))                                              \
{                                                              \
    tfm::printf("test failed at %s:%d\n", __FILE__, __LINE__); \
    tfm::printf("    %s != %s\n", (a), (b));                   \
    tfm::printf("    [%s, %s]\n", #a, #b);                     \
    std::abort();                                              \
}

