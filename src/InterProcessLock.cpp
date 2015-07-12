#include "InterProcessLock.h"

#ifdef _WIN32
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#else
#   include <unistd.h>
#   include <sys/file.h> // flock()
#   include <string.h>   // strerror()
#endif

#include <tinyformat.h>


#ifdef _WIN32

/// Windows implementation using creation of a windows mutex as the lock
class InterProcessLock::Impl
{
    public:
        Impl(const std::string& lockName)
            : m_mutex(NULL)
        {
            // Convert UTF-8 lock name to windows-native UTF-16
            m_lockName.resize(MultiByteToWideChar(CP_UTF8, 0, lockName.c_str(), -1, NULL, 0));
            MultiByteToWideChar(CP_UTF8, 0, lockName.c_str(), -1,
                                &m_lockName[0], (int)m_lockName.size());
        }

        ~Impl()
        {
            unlock();
        }

        bool tryLock()
        {
            if (m_mutex)
                return true;
            // CreateMutex() creates a new named mutex *or* gets a handle to
            // the existing mutex with the same name.
            m_mutex = CreateMutexW(NULL, FALSE, m_lockName.c_str());
            if (!m_mutex)
            {
                std::cerr << "Unexpected CreateMutex() failure: " << GetLastError() << "\n";
                return false;
            }
            // Expect to always get here with valid m_mutex.  If we were the
            // process which created it, consider that we obtained the lock.
            // Yes, this is a bit strange - we don't call WaitForSingleObject()
            // to actually lock the mutex at all!
            if (GetLastError() == ERROR_ALREADY_EXISTS)
            {
                // Didn't lock - close handle early so that when the process
                // with the lock exits the system will destroy the mutex
                // entirely.  This will allow another process to recreate it
                // and obtain the lock, regardless of whether there's a process
                // without the lock which is still hanging around.
                CloseHandle(m_mutex);
                m_mutex = NULL;
            }
            return m_mutex != NULL;
        }

        void unlock()
        {
            if (m_mutex)
            {
                CloseHandle(m_mutex);
                m_mutex = NULL;
            }
        }

    private:
        std::wstring m_lockName;
        HANDLE m_mutex;
};

#else


/// Posix implementation using a file lock
///
/// Amusing/depressing perspective about the borkenness of posix locking:
/// http://0pointer.de/blog/projects/locking.html
/// Luckily the usage below falls into the category of "trivial usage on a
/// very-probably-local filesystem"...
class InterProcessLock::Impl
{
    public:
        Impl(const std::string& lockName)
            : m_fd(-1)
        {
            std::string lockDir = "/tmp";
            if (const char* tmpDir = getenv("TMPDIR"))
                lockDir = tmpDir;
            m_lockPath = lockDir + "/" + lockName;
        }

        ~Impl()
        {
            unlock();
        }

        bool tryLock()
        {
            if (m_fd != -1)
                return true; // Already have lock
            // Open file
            while (true)
            {
                m_fd = open(m_lockPath.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
                if (m_fd != -1)
                    break;
                if (errno != EINTR)
                {
                    tfm::format(std::cerr, "Unexpected error opening lock file %s: %s\n",
                                m_lockPath, strerror(errno));
                    return false;
                }
            }
            // Try to lock without blocking
            while (flock(m_fd, LOCK_EX | LOCK_NB) == -1)
            {
                if (errno == EINTR)
                    continue;
                // Other errors cancel the lock attempt
                close(m_fd);
                m_fd = -1;
                if (errno == EWOULDBLOCK)
                {
                    /* another process has the lock */
                }
                else
                {
                    tfm::format(std::cerr, "Unexpected error locking file %s: %s\n",
                                m_lockPath, strerror(errno));
                }
                return false;
            }
            return true;
        }

        void unlock()
        {
            if (m_fd == -1)
                return;
            while (flock(m_fd, LOCK_UN) == -1 && errno == EINTR)
                ;
            while (close(m_fd) == -1 && errno == EINTR)
                ;
            unlink(m_lockPath.c_str());
            m_fd = -1;
        }

    private:
        int m_fd;
        std::string m_lockPath;
};

#endif


InterProcessLock::InterProcessLock(const std::string& instanceName)
    : m_impl(new Impl(instanceName))
{ }


InterProcessLock::~InterProcessLock()
{ }


bool InterProcessLock::tryLock()
{
    return m_impl->tryLock();
}

void InterProcessLock::unlock()
{
    m_impl->unlock();
}
