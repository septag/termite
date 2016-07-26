#pragma once

#include "bx/allocator.h"

namespace termite
{
    result_t initCommandSystem(uint16_t historySize, bx::AllocatorI* alloc);
    void shutdownCommandSystem();

    typedef bool(*ExecuteCommandFn)(const void* param);
    typedef void(*UndoCommandFn)(const void* param);
    typedef void(*CleanupCommandFn)(void* userData);

    struct CommandTypeT {};
    struct CommandT {};

    typedef PhantomType<uint32_t, CommandT, UINT32_MAX> CommandHandle;
    typedef PhantomType<uint16_t, CommandTypeT, UINT16_MAX> CommandTypeHandle;

    CommandTypeHandle registerCommand(const char* name, ExecuteCommandFn executeFn, UndoCommandFn undoFn, 
                                      CleanupCommandFn cleanupFn /*=nullptr*/, size_t paramSize);
    CommandTypeHandle findCommand(const char* name);
    
    CommandHandle addCommand(CommandTypeHandle handle, const void* param, const void* undoParam);
    CommandHandle addCommandGroup(CommandTypeHandle handle, int numCommands, 
                                  const void* const* params, const void* const* undoParams);
    void beginCommandChain();
    void addCommandChain(CommandTypeHandle handle, const void* param, const void* undoParam);
    CommandHandle endCommandChain();

    void executeCommand(CommandHandle handle);
    void undoCommand(CommandHandle handle);
    
    CommandHandle getCommandHistory();
    CommandHandle getPrevCommand(CommandHandle curHandle);
    const char* getCommandName(CommandHandle handle);
    void setCommandData(CommandHandle handle, void* userData);
} // namespace termite