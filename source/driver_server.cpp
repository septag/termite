#include "core.h"
#include "driver_server.h"

#include "bx/string.h"
#include "bxx/linked_list.h"
#include "bxx/pool.h"
#include "bxx/logger.h"

using namespace st;

namespace st
{
    struct drvDriver
    {
        typedef bx::ListNode<drvDriver*> LNode;

        char name[32];
        drvType type;
        void* data;
        uint32_t version;

        LNode lnode;
    };
}   // namespace st

struct DriverServer
{
    drvDriver::LNode* drivers;
    bx::Pool<drvDriver> driverPool;

    DriverServer()
    {
        drivers = nullptr;
    }
};

static DriverServer* gServer = nullptr;

int st::drvInit()
{
    if (gServer) {
        assert(false);
        return ST_ERR_ALREADY_INITIALIZED;
    }

    BX_BEGINP("Initializing Driver Server");
    bx::AllocatorI* alloc = coreGetAlloc();
    gServer = BX_NEW(alloc, DriverServer);
    if (!gServer) {
        BX_END_FATAL();
        return ST_ERR_OUTOFMEM;
    }

    if (!gServer->driverPool.create(20, alloc)) {
        BX_END_FATAL();
        return ST_ERR_OUTOFMEM;
    }

    BX_END_OK();
    return ST_OK;
}

void st::drvShutdown()
{
    if (!gServer) {
        assert(false);
        return;
    }

    BX_BEGINP("Shutting down Driver Server");
    bx::AllocatorI* alloc = coreGetAlloc();
    assert(alloc);

    gServer->driverPool.destroy();
    BX_DELETE(alloc, gServer);
    gServer = nullptr;
    BX_END_OK();
}

static drvDriver* createDriver(const char* name, uint32_t version, drvType type, void* data)
{
    drvDriver* d = gServer->driverPool.newInstance();
    if (!d)
        return nullptr;

    bx::strlcpy(d->name, name, sizeof(d->name));
    d->data = data;
    d->type = type;
    d->version = version;

    bx::addListNode(&gServer->drivers, &d->lnode, d);

    return d;
}

static void destroyDriver(drvDriver* drv)
{
    assert(drv);
    bx::removeListNode(&gServer->drivers, &drv->lnode);
    gServer->driverPool.deleteInstance(drv);
}

drvDriver* st::drvRegisterGraphics(gfxDriver* driver, const char* name, uint32_t version)
{
    return createDriver(name, version, drvType::GraphicsDriver, driver);
}

gfxDriver* st::drvGetGraphics(drvDriver* drv)
{
    if (drv->type != drvType::GraphicsDriver)
        return nullptr;

    return (gfxDriver*)drv->data;
}

drvDriver* st::drvRegisterRenderer(gfxRender* render, const char* name, uint32_t version)
{
    return createDriver(name, version, drvType::Renderer, render);
}

gfxRender* st::drvGetRenderer(drvDriver* drv)
{
    if (drv->type != drvType::Renderer)
        return nullptr;

    return (gfxRender*)drv->data;
}

drvDriver* st::drvFindHandleByName(const char* name)
{
    drvDriver::LNode* node = gServer->drivers;
    while (node) {
        drvDriver* drv = node->data;
        if (bx::stricmp(drv->name, name) == 0)
            return drv;
        node = node->next;
    }

    return nullptr;
}

int st::drvFindHandlesByType(drvType type, drvDriver** handles, int maxHandles)
{
    int count = 0;

    drvDriver::LNode* node = gServer->drivers;
    while (node) {
        drvDriver* drv = node->data;
        if (drv->type == type) {
            if (handles) {
                if (count <= maxHandles)
                    handles[count++] = drv;
            } else {
                count++;
            }
        }
        node = node->next;
    }

    return count;
}

uint32_t st::drvGetVersion(drvDriver* drv)
{
    return drv->version;
}

const char* st::drvGetName(drvDriver* drv)
{
    return drv->name;
}

void st::drvUnregister(drvDriver* drv)
{
    destroyDriver(drv);
}

