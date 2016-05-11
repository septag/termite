#pragma once

///
/// Spin-Locks using atomic primitives
/// Source: http://locklessinc.com/articles/locks/

#include "../bx/cpu.h"

namespace bx
{
    // Normal 'Fair-Lock'
    class Lock
    {
        BX_CLASS(Lock, 
                 NO_COPY, 
                 NO_ASSIGNMENT);
    public:
        Lock()
        {
        }

        void lock()
        {
            int32_t me = atomicFetchAndAdd<int32_t>(&m_data.s.users, 1);
            while (m_data.s.ticket != me)
                relaxCpu();
        }

        void unlock()
        {
            readWriteBarrier();
            m_data.s.ticket++;
        }

        bool tryLock()
        {
            int32_t me = m_data.s.users;
            int32_t menew = me + 1;
            int64_t cmp = (int64_t(me) << 32) + me;
            int64_t cmpnew = (int64_t(menew) << 32) + me;

            if (atomicCompareAndSwap<int64_t>(&m_data.u, cmp, cmpnew) == cmp)
                return true;

            return false;
        }

        bool canLock()
        {
            Data d = m_data;
            readWriteBarrier();
            return d.s.ticket == d.s.users;
        }

    private:
        union Data
        {
            volatile int64_t u;
            struct
            {
                volatile int32_t ticket;
                volatile int32_t users;
            } s;

			Data()
			{
				this->u = 0;
			}
        };

        Data m_data;
    };

    class LockScope
    {
        BX_CLASS(LockScope,
                 NO_ASSIGNMENT,
                 NO_COPY,
                 NO_DEFAULT_CTOR);

    public:
        explicit LockScope(Lock& _lock) : m_lock(_lock)
        {
            m_lock.lock();
        }

        ~LockScope()
        {
            m_lock.unlock();
        }

    private:
        Lock& m_lock;
    };

    // Read-Write Lock
    class RwLock
    {
        BX_CLASS(RwLock,
                 NO_ASSIGNMENT,
                 NO_COPY);

    public:
        RwLock()
        {
            m_data.u = 0;
        }

        void lockWrite()
        {
            int64_t me = atomicFetchAndAdd<int64_t>(&m_data.u, 0x100000000 /*1<<32*/);
            int16_t val = int16_t((me >> 32) & 0xffff);
            while (val != m_data.s.write)
                relaxCpu();
        }

        void unlockWrite()
        {
            Data d = m_data;
            readWriteBarrier();

            d.s.write++;
            d.s.read++;

            *(int32_t*)(&m_data) = d.us;
        }

        bool tryWriteLock()
        {
            int16_t me = m_data.s.users;
            int16_t menew = me + 1;
            int64_t read = int64_t(m_data.s.read) << 16;
            int64_t cmp = (int64_t(me) << 32) + read + me;
            int64_t cmpnew = (int64_t(menew) << 32) + read + me;

            if (atomicCompareAndSwap<int64_t>(&m_data.u, cmp, cmpnew) == cmp)
                return true;

            return false;
        }

        void lockRead()
        {
            int64_t me = atomicFetchAndAdd<int64_t>(&m_data.u, 0x100000000 /*1<<32*/);
            int16_t val = int16_t((me >> 32) & 0xffff);
            while (val != m_data.s.read)
                relaxCpu();
            m_data.s.read++;
        }

        void unlockRead()
        {
            atomicInc<int16_t>(&m_data.s.write);
        }

        bool tryReadLock()
        {
            int16_t me = m_data.s.users;
            int16_t write = m_data.s.write;
            int16_t menew = me + 1;
            int64_t cmp = (int64_t(me) << 32) + (int64_t(me) << 16) + write;
            int64_t cmpnew = (int64_t(menew) << 32) + (int64_t(menew) << 16) + write;
            if (atomicCompareAndSwap(&m_data.u, cmp, cmpnew) == cmp)
                return true;
            return false;
        }
        
    private:
        union Data
        {
            volatile int64_t u;
            volatile int32_t us;

            struct
            {
                volatile int16_t write;
                volatile int16_t read;
                volatile int16_t users;
            } s;

			Data()
			{
				this->u = 0;
			}
        };

        Data m_data;
    };

    class ReadLockScope
    {
        BX_CLASS(ReadLockScope,
                 NO_ASSIGNMENT,
                 NO_COPY,
                 NO_DEFAULT_CTOR);

    public:
        explicit ReadLockScope(RwLock& _lock) : m_lock(_lock)
        {
            m_lock.lockRead();
        }

        ~ReadLockScope()
        {
            m_lock.unlockRead();
        }

    private:
        RwLock& m_lock;
    };

    class WriteLockScope
    {
        BX_CLASS(WriteLockScope,
                 NO_ASSIGNMENT,
                 NO_COPY,
                 NO_DEFAULT_CTOR);

    public:
        explicit WriteLockScope(RwLock& _lock) : m_lock(_lock)
        {
            m_lock.lockWrite();
        }

        ~WriteLockScope()
        {
            m_lock.unlockWrite();
        }

    private:
        RwLock& m_lock;
    };
} // namespace bx
