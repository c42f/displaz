#include <fstream>

#include "InterProcessLock.h"
#include "util.h"

void touch(const std::string& fileName)
{
    std::ofstream f(fileName);
}

int main(int argc, char* argv[])
{
    if (argc < 2)
        return EXIT_FAILURE;
    std::string fileName = argv[1];
    InterProcessLock lk("InterProcessLock_test");
    if (!lk.tryLock())
    {
        touch(fileName + ".blocked");
        return 0;
    }
    // We have the lock: sleep to let other processes exit
    touch(fileName + ".acquired");
    milliSleep(10000);
    return 0;
}
