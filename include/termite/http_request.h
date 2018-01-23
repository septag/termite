#pragma once

#if termite_CURL

// Make Async HTTP requests
#include "bx/bx.h"
#include "bxx/string.h"
#include <functional>

// Fwd declare: Include "restclient-cpp/connection.h" to use this class
namespace RestClient
{
    class Connection;
    struct Response_t;
    typedef Response_t Response;
}

namespace tee
{
    struct HttpHeaderField
    {
        const char* name;
        const char* value;
    };

    // Http response callback. Gets called in the caller thread
    typedef void (*HttpResponseCallback)(int code, const char* body, const HttpHeaderField* headers, int numHeaders, 
                                         void* userData);

    // Connection callback is used for advanced requests , which you should include "restclient-cpp/connection.h" and use the methods
    // This function is called within async worker thread, so the user should only use 'conn' methods and work on userData in a thread-safe manner
    // User should return conn response back to async worker thread that will be reported in HttpResponseCallback
    // Example: conn->SetCertFile, conn->Set...., return conn->get(..);
    typedef const RestClient::Response& (*HttpConnectionCallback)(RestClient::Connection* conn, void* userData);

    namespace http {
        // Config
        TEE_API void setCert(const char* filepath, bool insecure = false);
        TEE_API void setKey(const char* filepath, const char* passphrase = nullptr);
        TEE_API void setTimeout(int timeoutSecs);
        TEE_API void setBaseUrl(const char* url);

        // 
        TEE_API void get(const char* url, HttpResponseCallback responseFn, void* userData = nullptr);
        TEE_API void post(const char* url, const char* contentType, const char* data, HttpResponseCallback responseFn, 
                          void* userData = nullptr);
        TEE_API void put(const char* url, const char* contentType, const char* data, HttpResponseCallback responseFn, 
                         void* userData = nullptr);
        TEE_API void del(const char* url, HttpResponseCallback responseFn, void* userData = nullptr);
        TEE_API void head(const char* url, HttpResponseCallback responseFn, void* userData = nullptr);

        TEE_API void request(const char* url, HttpConnectionCallback connFn, HttpResponseCallback responseFn, void* userData = nullptr);
    }
}

#endif
