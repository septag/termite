#include "pch.h"

#if BX_PLATFORM_ANDROID
#include "termite/android.h"

#include "internal.h"

#include <unwind.h>
#include <dlfcn.h>
#include <cxxabi.h>
#include <signal.h>

namespace tee {
    static JavaVM *gJavaVM = nullptr;
    static jclass gActivityClass = 0;
    static jobject gActivityObj = 0;
    static CrashCallback gCrashCallback = nullptr;
    static void *gCrashUserData = nullptr;

    struct AndroidBacktrace {
        void **current;
        void **end;
    };

    JniMethod
    android::findMethod(const char *methodName, const char *methodSig, const char *classPath,
                        JniMethodType::Enum type) {
        assert(gJavaVM);

        JniMethod m;
        bx::memSet(&m, 0x00, sizeof(m));

        if (type == JniMethodType::Method) {
            gJavaVM->AttachCurrentThread(&m.env, nullptr);
        } else if (type == JniMethodType::StaticMethod) {
            gJavaVM->GetEnv((void **) &m.env, JNI_VERSION_1_6);
        }

        if (m.env == nullptr) {
            BX_WARN("Calling Java method '%s' failed: Cannot retreive Java Env", methodName);
            return m;
        }

        if (classPath == nullptr) {
            m.cls = gActivityClass;
            m.obj = gActivityObj;
        } else {
            m.cls = m.env->FindClass(classPath);
            if (m.cls == 0) {
                BX_WARN("Calling Java method '%s' failed: Cannot find class '%s'", methodName,
                        classPath);
                return m;
            }
        }


        if (type == JniMethodType::Method) {
            m.methodId = m.env->GetMethodID(m.cls, methodName, methodSig);
        } else if (type == JniMethodType::StaticMethod) {
            m.methodId = m.env->GetStaticMethodID(m.cls, methodName, methodSig);
        }
        if (m.methodId == 0) {
            BX_WARN("Finding Java method '%s' failed: Cannot find method in class '%s'", methodName,
                    classPath);
            return m;
        }

        return m;
    }

    // These functions are defined in Java as jni native calls
    extern "C" JNIEXPORT void JNICALL
    Java_com_termite_util_Platform_termiteInitEngineVars(JNIEnv *env, jclass cls,
                                                         jobject obj, jstring dataDir,
                                                         jstring cacheDir, jstring packageVersion) {
        BX_UNUSED(cls);

        env->GetJavaVM(&gJavaVM);
        gActivityClass = (jclass) env->NewGlobalRef(env->GetObjectClass(obj));
        gActivityObj = env->NewGlobalRef(obj);

        const char *dataDirStr = env->GetStringUTFChars(dataDir, nullptr);
        const char *cacheDirStr = env->GetStringUTFChars(cacheDir, nullptr);
        const char *packageVersionStr = env->GetStringUTFChars(packageVersion, nullptr);

        platformSetVars(dataDirStr, cacheDirStr, packageVersionStr);

        env->ReleaseStringUTFChars(packageVersion, packageVersionStr);
        env->ReleaseStringUTFChars(cacheDir, cacheDirStr);
        env->ReleaseStringUTFChars(dataDir, dataDirStr);
    }


    extern "C" JNIEXPORT void JNICALL
    Java_com_termite_util_Platform_termiteSetGraphicsReset(JNIEnv *env, jclass cls) {
        platformSetGfxReset(true);
    }

    extern "C" JNIEXPORT void JNICALL
    Java_com_termite_util_Platform_termiteSetDeviceHardwareKey(JNIEnv *env, jclass cls,
                                                               jboolean value) {
        platformSetHardwareKey(value ? true : false);
    }

    extern "C" JNIEXPORT void JNICALL
    Java_com_termite_util_Platform_termiteSetDeviceInfo(JNIEnv *env, jclass cls,
                                                        jstring brand, jstring model,
                                                        jstring uniqueId,
                                                        jlong totalMem, jlong availMem,
                                                        jlong thresholdMem,
                                                        jint apiVersion) {
        BX_UNUSED(cls);

        HardwareInfo hwInfo;
        bx::memSet(&hwInfo, 0x0, sizeof(hwInfo));

        const char *strBrand = env->GetStringUTFChars(brand, nullptr);
        bx::strCopy(hwInfo.brand, sizeof(hwInfo.brand), strBrand);
        env->ReleaseStringUTFChars(brand, strBrand);

        const char *strModel = env->GetStringUTFChars(model, nullptr);
        bx::strCopy(hwInfo.model, sizeof(hwInfo.model), strModel);
        env->ReleaseStringUTFChars(model, strModel);

        const char *strUniqueId = env->GetStringUTFChars(uniqueId, nullptr);
        bx::strCopy(hwInfo.uniqueId, sizeof(hwInfo.uniqueId), strUniqueId);
        env->ReleaseStringUTFChars(uniqueId, strUniqueId);

        hwInfo.totalMem = totalMem;
        hwInfo.apiVersion = apiVersion;

        platformSetHwInfo(hwInfo);
    }

    static _Unwind_Reason_Code unwindCallback(struct _Unwind_Context *context, void *arg) {
        AndroidBacktrace *state = (AndroidBacktrace *) arg;
        uintptr_t pc = _Unwind_GetIP(context);
        if (pc) {
            if (state->current == state->end) {
                return _URC_END_OF_STACK;
            } else {
                *state->current++ = reinterpret_cast<void *>(pc);
            }
        }
        return _URC_NO_REASON;
    }

    void android::dumpStackTrace(bx::String<1024> &callstack) {
        BX_TRACE("Callstack: ");

        const int max = 100;
        void *buffer[max];

        AndroidBacktrace state;
        state.current = buffer;
        state.end = buffer + max;

        _Unwind_Backtrace(unwindCallback, &state);

        int count = (int) (state.current - buffer);

        for (int idx = 0; idx < count; idx++) {
            const void *addr = buffer[idx];
            const char *symbol = "";

            Dl_info info;
            if (dladdr(addr, &info) && info.dli_sname) {
                symbol = info.dli_sname;
            }
            int status = 0;
            char *demangled = __cxxabiv1::__cxa_demangle(symbol, 0, 0, &status);
            char callstr[256];
            bx::snprintf(callstr, sizeof(callstr), "%03d: %p %s",
                         idx,
                         addr,
                         (NULL != demangled && 0 == status) ?
                         demangled : symbol);
            BX_TRACE(callstr);
            callstack += callstr;
            callstack += "\n";

            if (NULL != demangled)
                free(demangled);
        }
    }
}
#endif

