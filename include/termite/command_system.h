#pragma once

#include "bx/allocator.h"

namespace tee
{
    struct CommandTypeT {};
    struct CommandT {};

    typedef PhantomType<uint32_t, CommandT, UINT32_MAX> CommandHandle;
    typedef PhantomType<uint16_t, CommandTypeT, UINT16_MAX> CommandTypeHandle;

    namespace cmd {
        typedef bool(*ExecuteCommandFn)(void* param);
        typedef void(*UndoCommandFn)(void* param);
        typedef void(*CleanupCommandFn)(void* param);

        TEE_API CommandTypeHandle registerCmd(const char* name, 
                                              ExecuteCommandFn executeFn, 
                                              UndoCommandFn undoFn,
                                              CleanupCommandFn cleanupFn /*=nullptr*/, 
                                              size_t paramSize);
        TEE_API CommandTypeHandle findCmd(const char* name);

        // Add single command to the queue
        TEE_API CommandHandle add(CommandTypeHandle handle, const void* param);

        // Add a group of functions with same type. And different parameters, trigger with one Command
        // Example: Moving a group of objects
        TEE_API CommandHandle addGroup(CommandTypeHandle handle, int numCommands, const void* const* params);

        // Begins a commands chain. command chains consists of multiple commands with different types, but executed at once
        // Example: Clone object into a position (Clone, Move, Rotate, Scale)
        TEE_API void beginChain();
        TEE_API void addChain(CommandTypeHandle handle, const void* param);
        TEE_API CommandHandle endChain();

        TEE_API void execute(CommandHandle handle);
        TEE_API void undo(CommandHandle handle);
        TEE_API void reset();

        TEE_API void undoLast();
        TEE_API void redoLast();
        TEE_API CommandHandle getLast();
        TEE_API CommandHandle getFirst();
        TEE_API CommandHandle getPrev(CommandHandle curHandle);
        TEE_API CommandHandle getNext(CommandHandle curHandle);
        TEE_API const char* getName(CommandHandle handle);

        template <typename Tx>
        CommandHandle add(CommandTypeHandle handle, const Tx& param)
        {
            return add(handle, (const void*)&param);
        }

        template <typename Tx>
        void addChain(CommandTypeHandle handle, const Tx& param)
        {
            addChain(handle, (const void*)&param);
        }
   }
} // namespace tee
