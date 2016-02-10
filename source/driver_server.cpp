#include "driver_server.h"

#include "bx/string.h"
#include "bxx/linked_list.h"
#include "bxx/pool.h"

struct Driver
{
    typedef bx::ListNode<Driver*> LNode;
    
    char name[32];
    srvDriverType type;
    st::srvHandle handle;
    LNode lnode;
};

struct DriverServer
{
    Driver::LNode* drivers;
    bx::Pool<Driver> driverPool;
    uint16_t driverId;

    DriverServer()
    {
        drivers = nullptr;
        driverId = 0;
    }
};

static DriverServer* gServer = nullptr;

int st::srvInit()
{
    if (gServer) {
        assert(false);
        return ST_ERR_ALREADY_INITIALIZED;
    }

    bx::AllocatorI* alloc = coreGetAlloc();
    gServer = BX_NEW(alloc, DriverServer);
    if (!gServer)
        return ST_ERR_OUTOFMEM;

    if (!gServer->driverPool.create(20, alloc))
        return ST_ERR_OUTOFMEM;

    return ST_OK;
}

void st::srvShutdown()
{
    if (!gServer) {
        assert(false);
        return;
    }

    bx::AllocatorI* alloc = coreGetAlloc();
    assert(alloc);

    gServer->driverPool.destroy();
    BX_DELETE(alloc, gServer);
}

st::srvHandle st::srvRegisterGraphicsDriver(st::gfxDriver* driver, const char* name)
{
    Driver* driver = gServer->driverPool.newInstance();
    if (!driver)
        return ST_INVALID_HANDLE;
    
    
    driver->handle.idx = ++gServer->driverId;

    
}

void st::srvUnregisterGraphicsDriver(srvHandle handle)
{

}

st::gfxDriver* st::srvGetGraphicsDriver(srvHandle handle)
{
    return nullptr;
}

st::srvHandle st::srvFindDriverHandleByName(const char* name)
{
    Driver::LNode* node = gServer->drivers;
    while (node) {
        Driver* drv = node->data;
        if (bx::stricmp(drv->name, name) == 0)
            return drv->handle;
        node = node->next;
    }

    return ST_INVALID_HANDLE;
}

int st::srvFindDriverHandleByType(srvDriverType type, srvHandle* handles, int maxHandles)
{
    return -1;
}
