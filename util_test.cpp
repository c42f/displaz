// Copyright (C) 2012, Chris J. Foster and the other authors and contributors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of the software's owners nor the names of its
//   contributors may be used to endorse or promote products derived from this
//   software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// (This is the BSD 3-clause license)

#include "util.h"

#include <cassert>
#include <cstdlib>

#define CHECK_EQUAL(a, b)                                  \
if(!((a) == (b)))                                          \
{                                                          \
    std::cout << "test failed, line " << __LINE__ << "\n"; \
    std::cout << (a) << " != " << (b) << "\n";             \
    std::cout << "[" #a ", " #b "]\n";                     \
    std::abort();                                          \
}

int identity(int i) { return i; }

void multi_partition_test()
{
    // Test multi_partition()
    int v[] = { 1, 1, 1, 0, 1, 2, 0, 0, 3, 3, 3 };
    int N = sizeof(v)/sizeof(v[0]);
    int* endIters[] = {0,0,0,0};
    int M = 4;

    multi_partition(v, v + N, &identity, endIters, M);

    int vExpect[] = { 0, 0, 0, 1, 1, 1, 1, 2, 3, 3, 3 };
    for (int i = 0; i < N; ++i)
        CHECK_EQUAL(v[i], vExpect[i]);

    int classEndInds[] = { 3, 7, 8, 11 };
    for (int i = 0; i < M; ++i)
        CHECK_EQUAL(&v[0] + classEndInds[i], endIters[i]);
}


int main()
{
    multi_partition_test();

    return 0;
}
