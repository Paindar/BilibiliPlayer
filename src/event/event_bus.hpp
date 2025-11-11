#pragma once

#if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201103L) || __cplusplus >= 201103L)
#define CXX11
#endif

#include <list>
#include <set>

#ifdef CXX11
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>
// Avoid polluting the global namespace in a header. Import only the needed std symbols.
using std::function;
using std::shared_ptr;
using std::weak_ptr;
using std::unique_ptr;
using std::mutex;
using std::lock_guard;
using std::atomic;
using std::make_shared;
using std::enable_shared_from_this;
#else
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/atomic/atomic.hpp>
using namespace boost;
#define noexcept throw()
#endif

class EventBus : public enable_shared_from_this<EventBus> {
private:
    template<typename EventType>
    struct SubscribeRecord {
    public:
        weak_ptr<void> subscriber;
        function<bool(EventType&)> handler;
    private:
        unsigned long long m_id;
        static atomic<unsigned long long> s_id;
    public:
        SubscribeRecord()
        {
        m_id = s_id.fetch_add(1, std::memory_order_relaxed);
        }
        unsigned long long GetId() { return m_id; }
        bool operator==(const SubscribeRecord<EventType>& other) const
        {
            return m_id == other.m_id;
        }
        bool operator<(const SubscribeRecord<EventType>& other) const
        {
            return m_id < other.m_id;
        }
    };
    template<typename EventType>
    struct Dispatcher {
        weak_ptr<EventBus> bus;
        mutex mtx;
        std::set<SubscribeRecord<EventType> > subscribers;
    };
    
private:
    const size_t m_id;
#ifdef CXX11
    EventBus(const size_t id) : m_id(id) { }
    EventBus() = delete;
    EventBus(const EventBus&) = delete;
    EventBus(EventBus&&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus& operator=(EventBus&&) = default;
#else
    EventBus(const size_t id) : m_id(id) { }
    EventBus();
    // Prevent copying/moving (C++03 style: declare but do not define)
    EventBus(const EventBus&);
    EventBus& operator=(const EventBus&);
#endif
private:
    template<typename EventType>
    shared_ptr<Dispatcher<EventType> > GetDispatcher() noexcept
    {
        typedef std::list<shared_ptr<Dispatcher<EventType> > > DispatchersMap;
        static DispatchersMap dispatchers;
        static mutex mtx;

        lock_guard<mutex> lock(mtx);

        typename DispatchersMap::iterator target = dispatchers.end();
        for (typename DispatchersMap::iterator it = dispatchers.begin(); it != dispatchers.end(); )
        {
            weak_ptr<EventBus> wp = (*it)->bus;
            if (wp.expired())
            {
                it = dispatchers.erase(it);
            }
            else
            {
                shared_ptr<EventBus> sp = wp.lock();
                if (sp->m_id == m_id)
                {
                    target = it;
                    break;
                }
                ++it;
            }
        }
        if (target != dispatchers.end())
        {
            return *target;
        }
        shared_ptr<Dispatcher<EventType> > dispatcher = make_shared<Dispatcher<EventType> >();
        dispatcher->bus = shared_from_this();
        dispatchers.push_back(dispatcher);

        return dispatcher;
    }
public:
    static unique_ptr<EventBus> Create() 
    {
    static atomic<int> indexCounter(0);
    int index = indexCounter.fetch_add(1, std::memory_order_relaxed);
        return unique_ptr<EventBus>(new EventBus(index));
    }
    size_t GetId() { return m_id;}
    template<typename EventType>
    bool Subscribe(shared_ptr<void> subscriber, function<bool(const EventType&)> handler) noexcept
    {
        shared_ptr<Dispatcher<EventType> > dispatcher = GetDispatcher<EventType>();
        lock_guard<mutex> lock(dispatcher->mtx);
        SubscribeRecord<EventType> record;
        record.subscriber = subscriber;
        record.handler = handler;
        dispatcher->subscribers.insert(record);
        return true;
    }
    template<typename EventType>
    bool Unsubscribe(weak_ptr<void> subscriber) noexcept
    {
        shared_ptr<Dispatcher<EventType> > dispatcher = GetDispatcher<EventType>();
        lock_guard<mutex> lock(dispatcher->mtx);
        for(typename std::set<SubscribeRecord<EventType> >::const_reverse_iterator it=dispatcher->subscribers.rbegin(); it!=dispatcher->subscribers.rend(); )
        {
            if (it->subscriber.expired())
            {
                it = dispatcher->subscribers.erase(it);
            }
            else
            {
                if (it->subscriber.lock() == subscriber.lock())
                {
                    dispatcher->subscribers.erase(it);
                    return true;
                }
                ++it;
            }
        }
        return false;
    }
    template<typename EventType>
    bool Post(EventType& evt)
    {
        shared_ptr<Dispatcher<EventType> > dispatcher = GetDispatcher<EventType>();
        lock_guard<mutex> lock(dispatcher->mtx);
        bool result = true;
        for(typename std::set<SubscribeRecord<EventType> >::iterator it=dispatcher->subscribers.begin(); it!=dispatcher->subscribers.end(); )
        {
            if (it->subscriber.expired())
            {
                dispatcher->subscribers.erase(it);
            }
            else
            {
                if (shared_ptr<void> sp = it->subscriber.lock())
                {
                    result &= it->handler(evt);
                }
                ++it;
            }
        }
        return result;
    }
    // Todo:
    // PostAsync
    // HasSubscribe
    // CleanExpiredSubscriber
};

template<typename EventType>
atomic<unsigned long long> EventBus::SubscribeRecord<EventType>::s_id(0);  // Initialization