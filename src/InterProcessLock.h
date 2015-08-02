#ifndef INTERPROCESS_LOCK_H_INCLUDED
#define INTERPROCESS_LOCK_H_INCLUDED

#include <memory>
#include <string>


/// Interprocess lock
///
/// Use this class to limit a given operation to a single processes.  For
/// example, to ensure only a single instance of a GUI application runs.
///
/// An acquired lock has process lifetime - the system will clean it up in the
/// event that the process crashes.
///
/// On unix, the implementation is based on the flock() API, so the lock is
/// shared between parent and child after a call to either fork() or execve().
/// In this case, both processes must exit or call unlock() before the lock is
/// released.
class InterProcessLock
{
    public:
        /// Create lock named with the UTF-8 string `lockName`
        InterProcessLock(const std::string& lockName);

        /// Calls unlock()
        ~InterProcessLock();

        /// Try to acquire the lock, returning immediately.
        ///
        /// Return true if the lock was acquired; return false if the lock
        /// could not be acquired immediately (ie, it's held by another
        /// process).
        bool tryLock();

        /// Inherit lock from a parent process.
        ///
        /// The lockId parameter provides a way pass a system dependent lock id
        /// from a parent to child process.  Depending on the system, the lock
        /// may already have been inherited during the creation of the child
        /// process, in which case this is just book keeping and hooks things
        /// up so that unlock() can be used.
        ///
        /// Return true if the lock was successfully inherited, false otherwise.
        bool inherit(const std::string& lockId);

        /// Release the lock and clean up.
        ///
        /// Do nothing if the lock wasn't previously acquired.
        void unlock();

        /// Format lock identifier as a string for passing to a child process.
        ///
        /// If currently unlocked, return the empty string.
        std::string makeLockId() const;

    private:
        class Impl;
        // Hidden system-dependent implementation
        std::unique_ptr<Impl> m_impl;
};


#endif // INTERPROCESS_LOCK_H_INCLUDED
