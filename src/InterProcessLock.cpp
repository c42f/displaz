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

#include "tinyformat.h"


#ifdef _WIN32

/// Windows implementation
class InterProcessLock::Impl
{
    public:
};

#else

/// Posix implementation
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
            // Lock without blocking
            while (flock(m_fd, LOCK_EX | LOCK_NB) == -1)
            {
                if (errno == EINTR)
                    continue;
                if (errno == EWOULDBLOCK)
                {
                    // Lock already held by another process
                    close(m_fd);
                    m_fd = -1;
                    return false;
                }
                else
                {
                    tfm::format(std::cerr, "Unexpected error locking file %s: %s\n",
                                m_lockPath, strerror(errno));
                    return false;
                }
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
