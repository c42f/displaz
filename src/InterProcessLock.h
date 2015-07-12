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

        /// Release the lock and clean up.
        ///
        /// Do nothing if the lock wasn't previously acquired.
        void unlock();

    private:
        class Impl;
        // Hidden system-dependent implementation
        std::unique_ptr<Impl> m_impl;
};


#endif // INTERPROCESS_LOCK_H_INCLUDED
