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
    typedef std::function<void(int code, const char* body, const HttpHeaderField* headers, int numHeaders)> HttpResponseCallback;

    // Connection callback is used for advanced requests , which you should include "restclient-cpp/connection.h" and use the methods
    // This function is called within async worker thread, so the user should only use 'conn' methods and work on userData in a thread-safe manner
    // User should return conn response back to async worker thread that will be reported in HttpResponseCallback
    typedef std::function<const RestClient::Response&(RestClient::Connection* conn)> HttpConnectionCallback;

    namespace http {
        TEE_API void get(const char* url, HttpResponseCallback responseFn);
        TEE_API void post(const char* url, const char* contentType, const char* data, HttpResponseCallback responseFn);
        TEE_API void put(const char* url, const char* contentType, const char* data, HttpResponseCallback responseFn);
        TEE_API void del(const char* url, HttpResponseCallback responseFn);
        TEE_API void head(const char* url, HttpResponseCallback responseFn);

        TEE_API void request(const char* url, HttpConnectionCallback connFn, HttpResponseCallback responseFn);
    }
}

#endif
