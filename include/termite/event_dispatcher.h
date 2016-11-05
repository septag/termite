#pragma once

#include "bx/allocator.h"

namespace termite
{
    struct Event;

    // Callbacks
    // Runs this callback in every 'runEventDispatcher' , if this function returns true, it means that event is triggered
    // and then runs the 'trigger' callback, and removes the event
    typedef bool(*RunEventCallback)(void* params, float dt);

    // Triggers when 'Run' callback returns true
    typedef void(*TriggerEventCallback)(void* userData);

    // 
    result_t initEventDispatcher(bx::AllocatorI* alloc);
    void shutdownEventDispatcher();

    void runEventDispatcher(float dt);

    TERMITE_API Event* registerEvent(RunEventCallback runCallback, TriggerEventCallback triggerCallback, 
                                     bool destroyOnTrigger = true,
                                     const void* runParams = nullptr, size_t paramsSize = 0, void* triggerUserData = nullptr);
    TERMITE_API void unregisterEvent(Event* ev);

    template <typename Ty>
    Event* registerEvent(RunEventCallback runCallback, TriggerEventCallback triggerCallback, bool destroyOnTrigger = true,
                       const Ty* runParams = nullptr, void* triggerUserData = nullptr)
    {
        return registerEvent(runCallback, triggerCallback, destroyOnTrigger, runParams, sizeof(Ty), triggerUserData);
    }

    
    TERMITE_API Event* registerTimerEvent(TriggerEventCallback callback, float interval, bool runOnce, void* userData = nullptr);
} // namespace termite

