#pragma once

#include "bx/allocator.h"

namespace tee
{
    struct Event;

    // Callbacks
    // Runs this callback in every 'runEventDispatcher' , if this function returns true, it means that event is triggered
    // and then runs the 'trigger' callback, and removes the event
    typedef bool(*RunEventCallback)(void* params, float dt);

    // Triggers when 'Run' callback returns true
    typedef void(*TriggerEventCallback)(void* userData);

    // API
    TEE_API Event* registerEvent(RunEventCallback runCallback, 
                                 TriggerEventCallback triggerCallback, 
                                 bool destroyOnTrigger = true,
                                 const void* runParams = nullptr, 
                                 size_t paramsSize = 0, 
                                 void* triggerUserData = nullptr);
    TEE_API void unregisterEvent(Event* ev);

    template <typename Ty>
    Event* registerEvent(RunEventCallback runCallback, TriggerEventCallback triggerCallback, bool destroyOnTrigger = true,
                       const Ty* runParams = nullptr, void* triggerUserData = nullptr)
    {
        return registerEvent(runCallback, triggerCallback, destroyOnTrigger, runParams, sizeof(Ty), triggerUserData);
    }

    
    TEE_API Event* registerTimerEvent(TriggerEventCallback callback, float interval, bool runOnce, void* userData = nullptr);
} // namespace tee

