#pragma once

#include <QMutex>
#include <QThread>
#include <QDebug>

// A small debug-checked mutex that mirrors QMutex API but records the
// owning thread in debug builds so we can assert ownership where needed.
class CheckedMutex {
public:
    CheckedMutex() = default;

    void lock() {
#ifndef NDEBUG
        m_mutex.lock();
        m_owner = QThread::currentThread();
        m_locked = true;
#else
        m_mutex.lock();
#endif
    }

    void unlock() {
#ifndef NDEBUG
        m_owner = nullptr;
        m_locked = false;
        m_mutex.unlock();
#else
        m_mutex.unlock();
#endif
    }

    bool tryLock(int timeout = 0) {
#ifndef NDEBUG
        bool ok = m_mutex.tryLock(timeout);
        if (ok) { m_owner = QThread::currentThread(); m_locked = true; }
        return ok;
#else
        return m_mutex.tryLock(timeout);
#endif
    }

    // Debug-only assertion to ensure current thread owns the mutex
    void assertOwned() const {
#ifndef NDEBUG
        QThread* cur = QThread::currentThread();
        Q_ASSERT(m_locked && m_owner == cur);
#else
        Q_UNUSED(this);
#endif
    }

private:
    mutable QMutex m_mutex;
#ifndef NDEBUG
    mutable QThread* m_owner = nullptr;
    mutable bool m_locked = false;
#endif
};

// Simple RAII locker for CheckedMutex (mirrors QMutexLocker interface used
// in the project).
class CheckedMutexLocker {
public:
    explicit CheckedMutexLocker(CheckedMutex* m) : m_mutex(m), m_owns(true) { m_mutex->lock(); }
    ~CheckedMutexLocker() { if (m_owns) m_mutex->unlock(); }
    // Allow explicit early unlock similar to QMutexLocker
    void unlock() { if (m_owns) { m_mutex->unlock(); m_owns = false; } }
    // Non-copyable
    CheckedMutexLocker(const CheckedMutexLocker&) = delete;
    CheckedMutexLocker& operator=(const CheckedMutexLocker&) = delete;
private:
    CheckedMutex* m_mutex;
    bool m_owns;
};
