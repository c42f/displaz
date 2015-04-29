// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt


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

