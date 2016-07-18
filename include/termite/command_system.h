#pragma once

#include "bx/allocator.h"

namespace termite
{
    result_t initCommandSystem(uint16_t historySize, bx::AllocatorI* alloc);
    void shutdownCommandSystem();

    typedef bool(*ExecuteCommandFn)(const void* param);
    typedef void(*UndoCommandFn)();

    struct CommandTypeT {};
    struct CommandT {};

    typedef PhantomType<uint16_t, CommandT, UINT16_MAX> CommandHandle;
    typedef PhantomType<uint16_t, CommandTypeT, UINT16_MAX> CommandTypeHandle;

    CommandTypeHandle registerCommand(const char* name, ExecuteCommandFn executeFn, UndoCommandFn undoFn, size_t paramSize);
    CommandTypeHandle findCommand(const char* name);
    
    CommandHandle executeCommand(CommandTypeHandle handle, const void* param);
    CommandHandle executeCommandGroup(CommandTypeHandle handle, int numCommands, const void* const* params);
    CommandHandle executeCommandChain(CommandTypeHandle handle, const void* param);
    void undoCommand(CommandHandle handle);
    
    CommandHandle getCommandHistory();
    CommandHandle getPrevCommand(CommandHandle curHandle);
    const char* getCommandName(CommandHandle handle);
} // namespace termite