#include "pch.h"

#if termite_CURL

#include "restclient-cpp/restclient.h"
#include "restclient-cpp/connection.h"

#include "http_request.h"
#include "bxx/queue.h"
#include "bx/thread.h"
#include "bx/mutex.h"
#include "bx/semaphore.h"
#include "tinystl/hash.h"

#include "internal.h"

namespace tee {
    struct HttpRequestMethod
    {
        enum Enum {
            GET,
            POST,
            PUT,
            DEL,
            HEAD
        };
    };

    struct HttpRequest
    {
        HttpRequestMethod::Enum method;
        HttpResponseCallback responseFn;

        char* url;
        char* contentType;
        char* data;

        HttpConnectionCallback* connFn;     // For advanced, or else this is nullptr
        bx::Queue<HttpRequest*>::Node qnode;

        HttpRequest() :
            method(HttpRequestMethod::GET),
            connFn(nullptr),
            qnode(this)
        {
            url[0] = 0;
        }

        ~HttpRequest()
        {
            if (connFn)
                connFn->~HttpConnectionCallback();
        }
    };

    struct HttpResponse
    {
        RestClient::Response r;
        HttpResponseCallback responseFn;
        bx::Queue<HttpResponse*>::Node qnode;

        HttpResponse() :
            qnode(this)
        {
        }
    };

    struct HttpRequestContext
    {
        bx::AllocatorI* alloc;
        bx::Thread workerThread;
        bx::Mutex reqMtx;
        bx::Mutex resMtx;
        bx::Queue<HttpRequest*> reqQueue;
        bx::Queue<HttpResponse*> resQueue;
        volatile int32_t quit;
        bx::Semaphore sem;

        RestClient::Connection* conn;
        size_t connUrlHash;

        HttpRequestContext(bx::AllocatorI* _alloc) :
            alloc(_alloc),
            quit(0),
            conn(nullptr),
            connUrlHash(0)
        {
        }
    };

    static HttpRequestContext* gHttp = nullptr;

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
                bx::MutexScope mtx(gHttp->reqMtx);
                if (!gHttp->reqQueue.pop(&req)) {
                    continue;
                }
            }

            // For each request, we have a response
            HttpResponse* res = BX_NEW(gHttp->alloc, HttpResponse);
            if (!res)
                continue;

            if (!req->connFn) {
                switch (req->method) {
                case HttpRequestMethod::GET:
                    res->r = RestClient::get(req->url);
                    break;

                case HttpRequestMethod::POST:
                    res->r = RestClient::post(req->url, req->contentType, req->data);
                    break;

                case HttpRequestMethod::PUT:
                    res->r = RestClient::put(req->url, req->contentType, req->data);
                    break;

                case HttpRequestMethod::DEL:
                    res->r = RestClient::del(req->url);
                    break;

                case HttpRequestMethod::HEAD:
                    res->r = RestClient::head(req->url);
                    break;
                }
            } else {
                size_t connUrlHash = tinystl::hash_string(req->url, strlen(req->url));
                if (connUrlHash != gHttp->connUrlHash) {
                    if (gHttp->conn)
                        BX_DELETE(gHttp->alloc, gHttp->conn);
                    gHttp->conn = BX_NEW(gHttp->alloc, RestClient::Connection)(req->url);
                } 
                res->r = (*req->connFn)(gHttp->conn);
            }              

            res->responseFn = req->responseFn;
            BX_DELETE(gHttp->alloc, req);

            bx::MutexScope mtx(gHttp->resMtx);
            gHttp->resQueue.push(&res->qnode);
        }


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

        if (gHttp->conn) {
            BX_DELETE(gHttp->alloc, gHttp->conn);
        }
        BX_DELETE(gHttp->alloc, gHttp);
        gHttp = nullptr;
    }

    void http::update()
    {
        // Fetch all requests and process them
        HttpResponse* res;
        gHttp->resMtx.lock();
        while (gHttp->resQueue.pop(&res)) {
            gHttp->resMtx.unlock();

            if (res->r.headers.size() > 0) {
                int numHeaders = (int)res->r.headers.size();
                HttpHeaderField* headers = (HttpHeaderField*)alloca(sizeof(HttpHeaderField)*numHeaders);
                int index = 0;
                for (std::map<std::string, std::string>::iterator it = res->r.headers.begin(); it != res->r.headers.end(); ++it) {
                    headers[index].name = it->first.c_str();
                    headers[index].value = it->second.c_str();
                    index++;
                }
                res->responseFn(res->r.code, res->r.body.c_str(), headers, numHeaders);
            } else {
                res->responseFn(res->r.code, res->r.body.c_str(), nullptr, 0);
            }

            BX_DELETE(gHttp->alloc, res);
        }
        gHttp->resMtx.lock();
    }

    static void makeNormalRequest(HttpRequestMethod::Enum method, const char* url, const char* contentType, 
                                  const char* data, HttpResponseCallback responseFn)
    {
        size_t urlSz = strlen(url) + 1;
        size_t contentTypeSz = (method == HttpRequestMethod::POST || method == HttpRequestMethod::PUT) ?
            strlen(contentType) + 1 : 0;
        size_t dataSz = (method == HttpRequestMethod::POST || method == HttpRequestMethod::PUT) ?
            strlen(data) + 1 : 0;
        size_t totalSz = sizeof(HttpRequest) + urlSz + contentTypeSz + dataSz;
        uint8_t* buff = (uint8_t*)BX_ALLOC(gHttp->alloc, totalSz);
        if (buff) {
            HttpRequest* req = new(buff) HttpRequest();
            buff += sizeof(HttpRequest);
            req->url = (char*)buff;
            strcpy(req->url, url);

            if (method == HttpRequestMethod::POST || method == HttpRequestMethod::PUT) {
                buff += urlSz;
                req->contentType = (char*)buff;
                buff += contentTypeSz;
                req->data = (char*)buff;

                strcpy(req->contentType, contentType);
                strcpy(req->data, data);
            }

            req->method = method;
            req->responseFn = responseFn;

            bx::MutexScope mtx(gHttp->reqMtx);
            gHttp->reqQueue.push(&req->qnode);
            gHttp->sem.post();
        }
    }

    void http::get(const char* url, HttpResponseCallback responseFn)
    {
        makeNormalRequest(HttpRequestMethod::GET, url, nullptr, nullptr, responseFn);
    }

    void http::post(const char* url, const char* contentType, const char* data, HttpResponseCallback responseFn)
    {
        makeNormalRequest(HttpRequestMethod::POST, url, contentType, data, responseFn);
    }

    void http::put(const char* url, const char* contentType, const char* data, HttpResponseCallback responseFn)
    {
        makeNormalRequest(HttpRequestMethod::PUT, url, contentType, data, responseFn);
    }

    void http::del(const char* url, HttpResponseCallback responseFn)
    {
        makeNormalRequest(HttpRequestMethod::DEL, url, nullptr, nullptr, responseFn);
    }

    void http::head(const char* url, HttpResponseCallback responseFn)
    {
        makeNormalRequest(HttpRequestMethod::HEAD, url, nullptr, nullptr, responseFn);
    }

    void http::request(const char* url, HttpConnectionCallback connFn, HttpResponseCallback responseFn)
    {
        size_t urlSz = strlen(url);
        size_t totalSz = sizeof(HttpRequest) + urlSz + 1;
        uint8_t* buff = (uint8_t*)BX_ALLOC(gHttp->alloc, totalSz);
        if (buff) {
            HttpRequest* req = new(buff) HttpRequest();
            buff += sizeof(HttpRequest);
            req->url = (char*)buff;
            buff += urlSz + 1;
            req->connFn = new(buff) HttpConnectionCallback;

            strcpy(req->url, url);
            
            req->responseFn = responseFn;
            *req->connFn = connFn;

            bx::MutexScope mtx(gHttp->reqMtx);
            gHttp->reqQueue.push(&req->qnode);
            gHttp->sem.post();
        }
    }

}   // namespace tee

#endif // termite_CURL