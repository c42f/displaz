#include <memory>
#include <vector>

#include <QCoreApplication>
#include <QProcess>

#include "InterProcessLock.h"
#include "util.h"

int main(int argc, char* argv[])
{
    if (argc < 2)
        return EXIT_FAILURE;
    if (argv[1] == std::string("master"))
    {
        QCoreApplication app(argc, argv);
        QString prog = QCoreApplication::applicationFilePath();
        std::vector<std::unique_ptr<QProcess>> procs;
        QStringList args;
        args << "slave";
        // Start a whole bunch of processes.  Unfortunately QtProcess isn't
        // robust enough to start more than about 100 at once on linux so
        // stress testing more requires a different strategy.
        const int nprocs = 100;
        for (int i = 0; i < nprocs; ++i)
        {
            std::unique_ptr<QProcess> proc(new QProcess());
            proc->start(prog, args);
            procs.push_back(std::move(proc));
        }
        // Wait for them all to finish, accumulating exit status
        int numAcquiredLocks = 0;
        int numBlockedLocks = 0;
        for (int i = 0; i < nprocs; ++i)
        {
            if (!procs[i]->waitForFinished() &&
                procs[i]->exitStatus() != QProcess::NormalExit)
            {
                std::cerr << "Error in waitForFinished()\n";
                return EXIT_FAILURE;
            }
            if (procs[i]->exitCode() == 0)
                ++numAcquiredLocks;
            else if (procs[i]->exitCode() == 1)
                ++numBlockedLocks;
        }
        tfm::printfln("Acquired locks: %d", numAcquiredLocks);
        tfm::printfln("Blocked locks: %d", numBlockedLocks);
        if (numAcquiredLocks != 1)
        {
            std::cerr << "Exactly one process should have got the lock\n";
            return EXIT_FAILURE;
        }
        if (numBlockedLocks != nprocs-1)
        {
            std::cerr << "Should have exactly N-1 blocked locks\n";
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
    else
    {
        // Slave process.  This is what we're trying to test.
        InterProcessLock lk("InterProcessLock_test");
        if (!lk.tryLock())
            return 1;
        // We have the lock: sleep to let other processes exit
        milliSleep(4000);
        return 0;
    }
}
