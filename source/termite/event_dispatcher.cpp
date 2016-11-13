#include "pch.h"

#include "event_dispatcher.h"

#include "bxx/linked_list.h"
#include "bxx/pool.h"
#include "bxx/logger.h"

using namespace termite;

#define MAX_PARAM_SIZE 256

namespace termite {
    struct Event
    {
        typedef bx::List<Event*>::Node LNode;
        RunEventCallback runCallback;
        TriggerEventCallback triggerCallback;
        bool destroyOnTrigger;
        void* triggerUserData;
        size_t paramsSize;
        uint8_t runParams[MAX_PARAM_SIZE];
        LNode lnode;

        Event() :
            runCallback(nullptr),
            triggerCallback(nullptr),
            destroyOnTrigger(true),
            triggerUserData(nullptr),
            paramsSize(0),
            lnode(this)
        {
        }
    };
}

struct EventDispatcher
{
    bx::AllocatorI* alloc;
    bx::List<Event*> eventList;
    bx::Pool<Event> eventPool;
    
    EventDispatcher(bx::AllocatorI* _alloc) :
        alloc(_alloc)
    {
    }
};

struct TimerEvent
{
    float elapsed;
    float interval;

    TimerEvent() :
        elapsed(0),
        interval(0)
    {
    }
};

static EventDispatcher* g_evDispatch = nullptr;

result_t termite::initEventDispatcher(bx::AllocatorI* alloc)
{
    if (g_evDispatch) {
        assert(0);
        return T_ERR_ALREADY_INITIALIZED;
    }

    g_evDispatch = BX_NEW(alloc, EventDispatcher)(alloc);
    if (!g_evDispatch)
        return T_ERR_OUTOFMEM;

    if (!g_evDispatch->eventPool.create(32, alloc)) {
        return T_ERR_OUTOFMEM;
    }

    return 0;
}

void termite::shutdownEventDispatcher()
{
    if (!g_evDispatch)
        return;

    g_evDispatch->eventPool.destroy();
    BX_DELETE(g_evDispatch->alloc, g_evDispatch);
    g_evDispatch = nullptr;
}

void termite::runEventDispatcher(float dt)
{
    Event::LNode* node = g_evDispatch->eventList.getFirst();
    while (node) {
        Event* ev = node->data;
        Event::LNode* next = node->next;

        if (ev->runCallback(ev->paramsSize > 0 ? ev->runParams : nullptr, dt)) {
            ev->triggerCallback(ev->triggerUserData);
            if (ev->destroyOnTrigger) {
                g_evDispatch->eventList.remove(node);
                g_evDispatch->eventPool.deleteInstance(ev);
            }
        }

        node = next;
    }
}

Event* termite::registerEvent(RunEventCallback runCallback, TriggerEventCallback triggerCallback, bool destroyOnTrigger,
                            const void* runParams, size_t paramsSize, void* triggerUserData)
{
    assert(paramsSize < MAX_PARAM_SIZE);

    Event* ev = g_evDispatch->eventPool.newInstance();
    if (!ev) {
        BX_WARN("Out of Memory");
        return nullptr;
    }
    
    ev->runCallback = runCallback;
    ev->paramsSize = paramsSize;
    if (runParams && paramsSize)
        memcpy(ev->runParams, runParams, paramsSize);

    ev->triggerCallback = triggerCallback;
    ev->triggerUserData = triggerUserData;

    ev->destroyOnTrigger = destroyOnTrigger;

    g_evDispatch->eventList.add(&ev->lnode);
    
    return ev;
}

Event* termite::registerTimerEvent(TriggerEventCallback callback, float interval, bool runOnce, void* userData)
{
    TimerEvent evParams;
    evParams.interval = interval;
    return registerEvent<TimerEvent>(
    [](void* params, float dt) {
        TimerEvent* timer = (TimerEvent*)params;
        timer->elapsed += dt;
        if (timer->elapsed < timer->interval)
            return false;
        timer->elapsed -= timer->interval;
        return true;
    }, callback, runOnce, &evParams, userData);
}

void termite::unregisterEvent(Event* ev)
{
    g_evDispatch->eventList.remove(&ev->lnode);
    g_evDispatch->eventPool.deleteInstance(ev);
}

