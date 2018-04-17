#include "pch.h"

#if termite_CURL

#include "restclient-cpp/restclient.h"
#include "restclient-cpp/connection.h"

#include "http_request.h"
#include "bxx/queue.h"
#include "bx/thread.h"
#include "bxx/lock.h"
#include "bx/semaphore.h"
#include "bxx/lock.h"
#include "tinystl/hash.h"

#include "internal.h"

namespace tee {
    struct HttpRequestMethod
    {
        enum Enum {
            Unknown = 0,
            GET,
            POST,
            PUT,
            DEL,
            HEAD
        };
    };

    struct HttpRequest
    {
        bx::AllocatorI* alloc;

        HttpRequestMethod::Enum method;
        HttpResponseCallback responseFn;

        HttpConnectionCallback connFn;     // For advanced mode (request), or else this is nullptr
        bx::Queue<HttpRequest*>::Node qnode;

        char url[256];
        char contentType[64];
        char* data;                        // For PUT/POST methods
        uint32_t dataSize;
        void* userData;

        HttpRequest() :
            method(HttpRequestMethod::Unknown),
            responseFn(nullptr),
            connFn(nullptr),
            qnode(this),
            data(nullptr),
            userData(nullptr)
        {
            url[0] = 0;
            contentType[0] = 0;
            dataSize = 0;
        }
    };

    struct HttpResponse
    {
        RestClient::Response r;
        HttpResponseCallback responseFn;
        bx::Queue<HttpResponse*>::Node qnode;
        void* userData;

        HttpResponse() :
            qnode(this),
            userData(nullptr),
            responseFn(nullptr)
        {
        }
    };

    struct HttpRequestContext
    {
        bx::AllocatorI* alloc;
        bx::Thread workerThread;
        bx::Lock reqLock;
        bx::Lock resLock;
        bx::Queue<HttpRequest*> reqQueue;
        bx::Queue<HttpResponse*> resQueue;
        volatile int32_t quit;
        bx::Semaphore sem;

        RestClient::Connection* conn;

        int timeout;
        bx::Path certFilepath;
        bx::Path keyFilepath;
        bx::String32 passphrase;
        char baseUrl[256];
        bool insecureCert;
        uint8_t padding[3];

        HttpRequestContext(bx::AllocatorI* _alloc) :
            alloc(_alloc),
            quit(0),
            conn(nullptr),
            timeout(10),
            insecureCert(true)
        {
            baseUrl[0] = 0;
        }
    };

    static HttpRequestContext* gHttp = nullptr;

    static void sendRequest(HttpRequest* req, HttpResponse* res)
    {
        // Create connection object
        size_t connUrlHash = tinystl::hash_string(req->url, strlen(req->url));
        RestClient::Connection* conn = BX_NEW(gHttp->alloc, RestClient::Connection)(gHttp->baseUrl);
        BX_ASSERT(conn, "");

        if (!gHttp->certFilepath.isEmpty()) {
            conn->SetCertPath(gHttp->certFilepath.cstr());
            conn->SetCertType("PEM");
        }

        if (!gHttp->keyFilepath.isEmpty()) {
            conn->SetKeyPath(gHttp->keyFilepath.cstr());
            conn->SetKeyPassword(gHttp->passphrase.cstr());
        }

        conn->SetTimeout(gHttp->timeout);
        conn->SetInsecureCert(gHttp->insecureCert);

        if (req->method != HttpRequestMethod::Unknown) {
            switch (req->method) {
            case HttpRequestMethod::GET:
                res->r = conn->get(req->url);
                break;

            case HttpRequestMethod::PUT:
                conn->AppendHeader("Content-Type", req->contentType);
                res->r = conn->put(req->url, req->data ? req->data : "");
                break;

            case HttpRequestMethod::POST:
                conn->AppendHeader("Content-Type", req->contentType);
                res->r = conn->post(req->url, req->data ? req->data : "", req->data ? req->dataSize : 1);
                break;

            case HttpRequestMethod::DEL:
                res->r = conn->del(req->url);
                break;

            case HttpRequestMethod::HEAD:
                res->r = conn->head(req->url);
                break;
            }
        } else {
            BX_ASSERT(req->connFn, "Custom request must have HttpConnectionCallback defined");
            res->r = req->connFn(conn, req->userData);
        }

        res->responseFn = req->responseFn;
        res->userData = req->userData;
        BX_DELETE(gHttp->alloc, conn);
    }

    static int32_t reqThread(bx::Thread* _self, void* userData)
    {
        assert(gHttp);
        RestClient::init();

        while (!gHttp->quit) {
            // Wait for a request 
            gHttp->sem.wait();

            // pull up one request and process it
            HttpRequest* req;
            {
                bx::LockScope mtx(gHttp->reqLock);
                if (!gHttp->reqQueue.pop(&req))
                    continue;
            }

            // For each request, we have a response
            HttpResponse* res = BX_NEW(gHttp->alloc, HttpResponse);
            if (!res)
                continue;
            sendRequest(req, res);

            BX_DELETE(gHttp->alloc, req);

            bx::LockScope mtx(gHttp->resLock);
            gHttp->resQueue.push(&res->qnode);
        }   // while NOT quit


        RestClient::disable();
        return 0;
    }

    bool http::init(bx::AllocatorI* alloc)
    {
        if (gHttp) {
            assert(false);
            return false;
        }

        gHttp = BX_NEW(alloc, HttpRequestContext)(alloc);
        gHttp->workerThread.init(reqThread, nullptr, 0, "HttpRequestWorker");

        return true;
    }

    void http::shutdown()
    {
        if (!gHttp)
            return;

        bx::atomicExchange<int32_t>(&gHttp->quit, 1);

        gHttp->sem.post(1);

        gHttp->workerThread.shutdown();

        BX_DELETE(gHttp->alloc, gHttp);
        gHttp = nullptr;
    }

    void http::update()
    {
        BX_ASSERT(gHttp, "");

        // Fetch all requests and process them
        HttpResponse* res;
        gHttp->resLock.lock();
        bool queueReady = gHttp->resQueue.pop(&res);
        gHttp->resLock.unlock();
        while (queueReady) {
            if (res->responseFn) {
                if (res->r.headers.size() > 0) {
                    int numHeaders = (int)res->r.headers.size();
                    HttpHeaderField* headers = nullptr;
                    if (numHeaders > 0) {
                        headers = (HttpHeaderField*)alloca(sizeof(HttpHeaderField)*numHeaders);
                        int index = 0;
                        for (std::map<std::string, std::string>::iterator it = res->r.headers.begin(); it != res->r.headers.end(); ++it) {
                            headers[index].name = it->first.c_str();
                            headers[index].value = it->second.c_str();
                            index++;
                        }
                    }
                    res->responseFn(res->r.code, res->r.body.c_str(), headers, numHeaders, res->userData);
                } else {
                    res->responseFn(res->r.code, res->r.body.c_str(), nullptr, 0, res->userData);
                }
            }

            BX_DELETE(gHttp->alloc, res);

            gHttp->resLock.lock();
            queueReady = gHttp->resQueue.pop(&res);
            gHttp->resLock.unlock();
        } 
    }

    static void makeRequest(HttpRequestMethod::Enum method, const char* url, 
                            const char* contentType, const char* data, uint32_t size, 
                            HttpResponseCallback responseFn, HttpConnectionCallback connFn, void* userData)
    {
        size_t dataSz = data ? size : 0;
        size_t totalSz = sizeof(HttpRequest) + dataSz;
        uint8_t* buff = (uint8_t*)BX_ALLOC(gHttp->alloc, totalSz);
        if (buff) {
            HttpRequest* req = BX_PLACEMENT_NEW(buff, HttpRequest);
            buff += sizeof(HttpRequest);

            bx::strCopy(req->url, sizeof(req->url), url);

            req->method = method;
            req->responseFn = responseFn;
            req->connFn = connFn;
            req->userData = userData;
            req->dataSize = size;

            if (contentType)
                bx::strCopy(req->contentType, sizeof(req->contentType), contentType);

            if (data) {
                req->data = (char*)buff;
                bx::memCopy(req->data, data, dataSz);
            }

            bx::LockScope mtx(gHttp->reqLock);
            gHttp->reqQueue.push(&req->qnode);
            gHttp->sem.post();
        }
    }

    static void makeRequestSync(HttpRequestMethod::Enum method, const char* url, const char* contentType,
                                const char* data, uint32_t size, HttpResponseCallback responseFn,
                                HttpConnectionCallback connFn, void* userData)
    {
        HttpResponse* res = BX_NEW(gHttp->alloc, HttpResponse);
        if (!res)
            return;

        size_t dataSz = data ? size : 0;
        size_t totalSz = sizeof(HttpRequest) + dataSz;
        uint8_t* buff = (uint8_t*)BX_ALLOC(gHttp->alloc, totalSz);
        HttpRequest* req = nullptr;
        if (buff) {
            req = BX_PLACEMENT_NEW(buff, HttpRequest);
            buff += sizeof(HttpRequest);

            bx::strCopy(req->url, sizeof(req->url), url);

            req->method = method;
            req->responseFn = responseFn;
            req->connFn = connFn;
            req->userData = userData;

            if (contentType)
                bx::strCopy(req->contentType, sizeof(req->contentType), contentType);

            if (data) {
                req->data = (char*)buff;
                bx::memCopy(req->data, data, dataSz);
            }
        } else {
            return;
        }

        sendRequest(req, res);

        if (res->responseFn) {
            if (res->r.headers.size() > 0) {
                int numHeaders = (int)res->r.headers.size();
                HttpHeaderField* headers = (HttpHeaderField*)alloca(sizeof(HttpHeaderField)*numHeaders);
                int index = 0;
                for (std::map<std::string, std::string>::iterator it = res->r.headers.begin(); it != res->r.headers.end(); ++it) {
                    headers[index].name = it->first.c_str();
                    headers[index].value = it->second.c_str();
                    index++;
                }
                res->responseFn(res->r.code, res->r.body.c_str(), headers, numHeaders, res->userData);
            } else {
                res->responseFn(res->r.code, res->r.body.c_str(), nullptr, 0, res->userData);
            }
        }

        BX_DELETE(gHttp->alloc, res);
        BX_DELETE(gHttp->alloc, req);
    }

    void http::get(const char* url, HttpResponseCallback responseFn, void* userData)
    {
        makeRequest(HttpRequestMethod::GET, url, nullptr, nullptr, 0, responseFn, nullptr, userData);
    }

    void http::post(const char* url, const char* contentType, const char* data, HttpResponseCallback responseFn, void* userData)
    {
        makeRequest(HttpRequestMethod::POST, url, contentType, data, data ? (uint32_t)strlen(data) + 1 : 0, responseFn, nullptr, userData);
    }

    void http::post(const char* url, const char* contentType, const char* binaryData, const uint32_t dataSize, 
                    HttpResponseCallback responseFn, void* userData /*= nullptr*/)
    {
        makeRequest(HttpRequestMethod::POST, url, contentType, binaryData, dataSize, responseFn, nullptr, userData);
    }

    void http::put(const char* url, const char* contentType, const char* data, HttpResponseCallback responseFn, void* userData)
    {
        makeRequest(HttpRequestMethod::PUT, url, contentType, data, data ? (uint32_t)strlen(data) + 1 : 0, responseFn, nullptr, userData);
    }

    void http::del(const char* url, HttpResponseCallback responseFn, void* userData)
    {
        makeRequest(HttpRequestMethod::DEL, url, nullptr, nullptr, 0, responseFn, nullptr, userData);
    }

    void http::head(const char* url, HttpResponseCallback responseFn, void* userData)
    {
        makeRequest(HttpRequestMethod::HEAD, url, nullptr, nullptr, 0, responseFn, nullptr, userData);
    }

    void http::request(const char* url, HttpConnectionCallback connFn, HttpResponseCallback responseFn, void* userData)
    {
        makeRequest(HttpRequestMethod::Unknown, url, nullptr, nullptr, 0, responseFn, connFn, userData);
    }

    void http::getSync(const char* url, HttpResponseCallback responseFn, void* userData)
    {
        makeRequestSync(HttpRequestMethod::GET, url, nullptr, nullptr, 0, responseFn, nullptr, userData);
    }

    void http::postSync(const char* url, const char* contentType, const char* data, HttpResponseCallback responseFn, void* userData)
    {
        makeRequestSync(HttpRequestMethod::POST, url, contentType, data, data ? (uint32_t)strlen(data) + 1 : 0, responseFn, nullptr, userData);
    }

    void http::putSync(const char* url, const char* contentType, const char* data, HttpResponseCallback responseFn, void* userData)
    {
        makeRequestSync(HttpRequestMethod::PUT, url, contentType, data, data ? (uint32_t)strlen(data) + 1 : 0, responseFn, nullptr, userData);
    }

    void http::delSync(const char* url, HttpResponseCallback responseFn, void* userData)
    {
        makeRequestSync(HttpRequestMethod::DEL, url, nullptr, nullptr, 0, responseFn, nullptr, userData);
    }

    void http::headSync(const char* url, HttpResponseCallback responseFn, void* userData)
    {
        makeRequestSync(HttpRequestMethod::HEAD, url, nullptr, nullptr, 0, responseFn, nullptr, userData);
    }

    void http::requestSync(const char* url, HttpConnectionCallback connFn, HttpResponseCallback responseFn, void* userData)
    {
        makeRequestSync(HttpRequestMethod::Unknown, url, nullptr, nullptr, 0, responseFn, connFn, userData);
    }

    void http::setCert(const char* filepath, bool insecure)
    {
        gHttp->certFilepath = filepath;

    }

    void http::setKey(const char* filepath, const char* passphrase /*= nullptr*/)
    {
        gHttp->keyFilepath = filepath;
        if (passphrase)
            gHttp->passphrase = passphrase;
    }

    void http::setTimeout(int timeoutSecs)
    {
        gHttp->timeout = timeoutSecs;
    }

    void http::setBaseUrl(const char* url)
    {
        bx::strCopy(gHttp->baseUrl, sizeof(gHttp->baseUrl), url);
    }

    bool http::isRequestFailed(int code)
    {
        return code == TEE_HTTP_OPERATION_TIMEOUT || code == TEE_HTTP_CERT_ERROR || code == TEE_HTTP_FAILED;
    }

}   // namespace tee

#endif // termite_CURL