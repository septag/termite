#include "pch.h"
#include "command_system.h"

#include "bxx/indexed_pool.h"
#include "bxx/array.h"
#include "bxx/hash_table.h"

using namespace termite;

struct CommandType
{
    char name[32];
    ExecuteCommandFn executeFn;
    UndoCommandFn undoFn;
    CleanupCommandFn cleanupFn;
    size_t paramSize;
    bx::IndexedPool paramPool;

    CommandType()
    {
        strcpy(name, "");
        executeFn = nullptr;
        undoFn = nullptr;
        cleanupFn = nullptr;
        paramSize = 0;
    }
};

struct Command
{
    CommandTypeHandle ctHandle;
    CommandHandle nextHandle;
    CommandHandle prevHandle;
    CommandHandle childHandle;
    uint16_t paramIndex;

    Command()
    {
        paramIndex = UINT16_MAX;
    }
};

struct CommandSystem
{
    bx::Array<CommandType> commandTypes;
    bx::IndexedPool commandPool;
    uint16_t maxSize;
    bx::AllocatorI* alloc;
    bx::HashTableInt commandTypeTable;
    CommandHandle topCommand;
    CommandHandle bottomCommand;

    CommandSystem(bx::AllocatorI* _alloc) :
        alloc(_alloc),
        commandTypeTable(bx::HashTableType::Mutable)
    {
        maxSize = 0;
    }
};

static CommandSystem* g_cmdSys = nullptr;

result_t termite::initCommandSystem(uint16_t historySize, bx::AllocatorI* alloc)
{
    assert(historySize);

    if (g_cmdSys) {
        assert(false);
        return T_ERR_ALREADY_INITIALIZED;
    }

    g_cmdSys = BX_NEW(alloc, CommandSystem)(alloc);
    if (!g_cmdSys)
        return T_ERR_OUTOFMEM;

    g_cmdSys->maxSize = historySize;

    const uint32_t itemSizes[] = {
        sizeof(Command)
    };
    
    if (!g_cmdSys->commandTypes.create(128, 256, alloc) ||
        !g_cmdSys->commandTypeTable.create(128, alloc) ||
        !g_cmdSys->commandPool.create(itemSizes, 1, historySize, historySize, alloc))
    {
        return T_ERR_OUTOFMEM;
    }

    return 0;
}

void termite::shutdownCommandSystem()
{
    if (!g_cmdSys)
        return;

    // Destroy all registered command types
    for (int i = 0; i < g_cmdSys->commandTypes.getCount(); i++) {
        CommandType* ctype = g_cmdSys->commandTypes.itemPtr(i);
        ctype->paramPool.destroy();
    }

    g_cmdSys->commandTypeTable.destroy();
    g_cmdSys->commandPool.destroy();
    g_cmdSys->commandTypes.destroy();

    BX_DELETE(g_cmdSys->alloc, g_cmdSys);
    g_cmdSys = nullptr;
}

CommandTypeHandle termite::registerCommand(const char* name, ExecuteCommandFn executeFn, UndoCommandFn undoFn, 
                                           CleanupCommandFn cleanupFn, size_t paramSize)
{
    assert(g_cmdSys);

    CommandType* ptr = g_cmdSys->commandTypes.push();
    if (!ptr)
        return CommandTypeHandle();

    CommandType* ctype = new(ptr) CommandType();

    bx::strlcpy(ctype->name, name, sizeof(ctype->name));
    ctype->executeFn = executeFn;
    ctype->undoFn = undoFn;
    ctype->cleanupFn = cleanupFn;
    
    ctype->paramSize = paramSize;

    if (paramSize > 0) {
        const uint32_t itemSizes[] = {
            uint32_t(paramSize)
        };
        if (!ctype->paramPool.create(itemSizes, 1, 32, 128, g_cmdSys->alloc))
            return CommandTypeHandle();
    }

    return CommandTypeHandle(uint16_t(g_cmdSys->commandPool.getCount() - 1));
}

CommandTypeHandle termite::findCommand(const char* name)
{
    assert(g_cmdSys);

    int r = g_cmdSys->commandTypeTable.find(bx::hashMurmur2A(name, (uint32_t)strlen(name)));
    if (r != -1) {
        int index = g_cmdSys->commandTypeTable.getValue(r);
        return CommandTypeHandle(uint16_t(index));
    }

    return CommandTypeHandle();
}

CommandHandle termite::executeCommand(CommandTypeHandle handle, const void* param)
{
    assert(handle.isValid());

    CommandType& ctype = g_cmdSys->commandTypes[handle.value];
    uint16_t cIndex = g_cmdSys->commandPool.newHandle();

    void* ptr = ctype.paramPool.getHandleData(0, cIndex);
    Command* cmd = new(ptr) Command();
    cmd->ctHandle = handle;

    void* params;
    if (ctype.paramSize > 0) {
        cmd->paramIndex = ctype.paramPool.newHandle();
        params = ctype.paramPool.getHandleData(0, cmd->paramIndex);
    } else {
        params = nullptr;
    }

    // TODO: Add to command list (stack)

    // TODO: Remove from the last item
        
    // Run execute
    ctype.executeFn(params);

    return CommandHandle(cIndex);
}

CommandHandle termite::executeCommandGroup(CommandTypeHandle handle, int numCommands, const void* const* params)
{
    return CommandHandle();
}

CommandHandle termite::executeCommandChain(CommandTypeHandle handle, const void* param)
{
    return CommandHandle();
}

void termite::undoCommand(CommandHandle handle)
{

}

CommandHandle termite::getCommandHistory()
{
    return g_cmdSys->topCommand;
}

CommandHandle termite::getPrevCommand(CommandHandle curHandle)
{
    assert(g_cmdSys);
    assert(curHandle.isValid());

    Command* cmd = g_cmdSys->commandPool.getHandleData<Command>(0, curHandle.value);
    return cmd->prevHandle;
}

const char* termite::getCommandName(CommandHandle handle)
{
    assert(g_cmdSys);
    assert(handle.isValid());

    Command* cmd = g_cmdSys->commandPool.getHandleData<Command>(0, handle.value);
    return g_cmdSys->commandTypes[cmd->ctHandle.value].name;
}
