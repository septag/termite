#include "pch.h"

#include "driver_mgr.h"

#include "bx/string.h"
#include "bxx/linked_list.h"
#include "bxx/pool.h"
#include "bxx/logger.h"

using namespace termite;

namespace termite
{
    struct drvDriver
    {
        typedef bx::ListNode<drvHandle> LNode;

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

int termite::drvInit()
{
    if (gServer) {
        assert(false);
        return T_ERR_ALREADY_INITIALIZED;
    }

    BX_BEGINP("Initializing Driver Manager");
    bx::AllocatorI* alloc = coreGetAlloc();
    gServer = BX_NEW(alloc, DriverServer);
    if (!gServer) {
        BX_END_FATAL();
        return T_ERR_OUTOFMEM;
    }

    if (!gServer->driverPool.create(20, alloc)) {
        BX_END_FATAL();
        return T_ERR_OUTOFMEM;
    }

    BX_END_OK();
    return T_OK;
}

void termite::drvShutdown()
{
    if (!gServer)
        return;

    BX_BEGINP("Shutting down Driver Manager");
    bx::AllocatorI* alloc = coreGetAlloc();
    assert(alloc);

    gServer->driverPool.destroy();
    BX_DELETE(alloc, gServer);
    gServer = nullptr;
    BX_END_OK();
}

drvHandle termite::drvRegister(drvType type, const char* name, uint32_t version, void* driver)
{
    drvHandle d = gServer->driverPool.newInstance();
    if (!d)
        return nullptr;

    bx::strlcpy(d->name, name, sizeof(d->name));
    d->data = driver;
    d->type = type;
    d->version = version;

    bx::addListNode(&gServer->drivers, &d->lnode, d);

    return d;
}

static void destroyDriver(drvHandle drv)
{
    assert(drv);
    bx::removeListNode(&gServer->drivers, &drv->lnode);
    gServer->driverPool.deleteInstance(drv);
}

gfxDriverI* termite::drvGetGraphicsDriver(drvHandle drv)
{
    if (drv->type != drvType::GraphicsDriver)
        return nullptr;

    return (gfxDriverI*)drv->data;
}

gfxRenderI* termite::drvGetRenderer(drvHandle drv)
{
    if (drv->type != drvType::Renderer)
        return nullptr;

    return (gfxRenderI*)drv->data;
}


dsDriverI* termite::drvGetDataStoreDriver(drvHandle drv)
{
    if (drv->type != drvType::DataStoreDriver)
        return nullptr;

    return (dsDriverI*)drv->data;
}

termite::drvHandle termite::drvFindHandleByName(const char* name)
{
    drvDriver::LNode* node = gServer->drivers;
    while (node) {
        drvHandle drv = node->data;
        if (bx::stricmp(drv->name, name) == 0)
            return drv;
        node = node->next;
    }

    return nullptr;
}

termite::drvHandle termite::drvFindHandleByPtr(void* driver)
{
    drvDriver::LNode* node = gServer->drivers;
    while (node) {
        drvHandle drv = node->data;
        if (drv->data == driver)
            return drv;
        node = node->next;
    }

    return nullptr;
}


int termite::drvFindHandlesByType(drvType type, drvHandle* handles, int maxHandles)
{
    int count = 0;

    drvDriver::LNode* node = gServer->drivers;
    while (node) {
        drvHandle drv = node->data;
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

uint32_t termite::drvGetVersion(drvHandle drv)
{
    if (!drv)
        return 0;
    return drv->version;
}

const char* termite::drvGetName(drvHandle drv)
{
    if (!drv)
        return "";

    return drv->name;
}

void termite::drvUnregister(drvHandle drv)
{
    destroyDriver(drv);
}


