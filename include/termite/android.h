#pragma once

#if BX_PLATFORM_ANDROID
#include <jni.h>
#include "bxx/string.h"

namespace tee {
    struct JniMethod
    {
        JNIEnv* env;
        jclass cls;
        jobject obj;
        jmethodID methodId;
    };

    struct JniMethodType
    {
        enum Enum {
            Method,
            StaticMethod
        };
    };

    typedef void (*CrashCallback)(void* userData);

    namespace android {
        // Calls a method in java
        // for classPath, methodName and methodSig parameters, see:
        //      http://journals.ecs.soton.ac.uk/java/tutorial/native1.1/implementing/method.html
        TEE_API JniMethod findMethod(const char* methodName, const char* methodSig, const char* classPath = nullptr,
                                     JniMethodType::Enum type = JniMethodType::Method);

        TEE_API void detachJni();
        TEE_API void dumpStackTrace(bx::String<1024>& callstack);
    }
}
#endif