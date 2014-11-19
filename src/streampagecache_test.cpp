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


#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <tinyformat.h>
#include "streampagecache.h"

#define CHECK(cond)                                                         \
if(!(cond))                                                                 \
{                                                                           \
    std::cout << "test CHECK(" #cond ") failed, line " << __LINE__ << "\n"; \
    std::abort();                                                           \
}

//void checkEqual(char* a, char* b, size_t len)
//{
//    if (std::memcmp(a, b, len) != 0)
//    {
//        tfm::printf("Buffers not equal\n");
//        for (size_t i = 0; i < len; ++i)
//        {
//            if (a[i] != b[i])
//                tfm::printf("a[%d] = %d, b[%d] = %d\n", i, a[i], i, b[i]);
//        }
//        std::abort();
//    }
//}

int main()
{
    const size_t size = 12345;
    char buf[size];
    for (size_t i = 0; i < size; ++i)
        buf[i] = rand() % 256;
    std::string tmpFileName = "streampagecache_test.dat";
    {
        std::ofstream out(tmpFileName, std::ios::binary);
        out.write(buf, size);
    }

    std::ifstream in(tmpFileName, std::ios::binary);
    StreamPageCache cache(in, 1001);

    char buf2[size] = {0};

    CHECK(!cache.prefetch(900, 200));
    cache.fetchNow(2);
    CHECK(cache.read(buf2, 900, 200));
    //checkEqual(buf + 900, buf2, 200);
    CHECK(std::memcmp(buf + 900, buf2, 200) == 0);

    for (size_t i = 0; i < size-3; ++i)
    {
        if (!cache.prefetch(i, 3))
            cache.fetchNow(2);
        CHECK(cache.read(buf2, i, 3));
        CHECK(std::memcmp(buf + i, buf2, 3) == 0);
    }

    return 0;
}

