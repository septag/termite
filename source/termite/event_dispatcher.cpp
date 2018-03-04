#include "pch.h"

#include "event_dispatcher.h"
#include "internal.h"

#include "bxx/linked_list.h"
#include "bxx/pool.h"
#include "bxx/logger.h"

#define MAX_PARAM_SIZE 256

namespace tee {
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

    static EventDispatcher* gEvents = nullptr;

    bool initEventDispatcher(bx::AllocatorI* alloc)
    {
        if (gEvents) {
            assert(0);
            return false;
        }

        gEvents = BX_NEW(alloc, EventDispatcher)(alloc);
        if (!gEvents)
            return false;

        if (!gEvents->eventPool.create(32, alloc)) {
            return false;
        }

        return true;
    }

    void shutdownEventDispatcher()
    {
        if (!gEvents)
            return;

        gEvents->eventPool.destroy();
        BX_DELETE(gEvents->alloc, gEvents);
        gEvents = nullptr;
    }

    void runEventDispatcher(float dt)
    {
        Event::LNode* node = gEvents->eventList.getFirst();
        while (node) {
            Event* ev = node->data;
            Event::LNode* next = node->next;

            if (ev->runCallback(ev->paramsSize > 0 ? ev->runParams : nullptr, dt)) {
                ev->triggerCallback(ev->triggerUserData);
                if (ev->destroyOnTrigger) {
                    gEvents->eventList.remove(node);
                    gEvents->eventPool.deleteInstance(ev);
                }
            }

            node = next;
        }
    }

    Event* registerEvent(RunEventCallback runCallback, TriggerEventCallback triggerCallback, bool destroyOnTrigger,
                         const void* runParams, size_t paramsSize, void* triggerUserData)
    {
        assert(paramsSize < MAX_PARAM_SIZE);

        Event* ev = gEvents->eventPool.newInstance();
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

        gEvents->eventList.add(&ev->lnode);

        return ev;
    }

    Event* registerTimerEvent(TriggerEventCallback callback, float interval, bool runOnce, void* userData)
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

    void unregisterEvent(Event* ev)
    {
        gEvents->eventList.remove(&ev->lnode);
        gEvents->eventPool.deleteInstance(ev);
    }

} // namespace tee