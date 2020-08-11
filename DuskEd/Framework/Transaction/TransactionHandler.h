/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include "TransactionCommand.h"

#include "Core/Allocators/PoolAllocator.h"

class BaseAllocator;

class TransactionHandler
{
public:
                                TransactionHandler( BaseAllocator* allocator );
                                TransactionHandler( TransactionHandler& ) = delete;
                                TransactionHandler& operator = ( TransactionHandler& ) = delete;
                                ~TransactionHandler();

    template<typename T, typename... TArgs>
    void commit( TArgs... args )
    {
        if ( commandCount >= MAX_HISTORY_SIZE ) {
            DUSK_LOG_ERROR( "Transaction Commands limit reached (%i >= %i)\n", commandCount, MAX_HISTORY_SIZE );
            return;
        }

        commandCount++;
        commandIdx++;

        // If we are in the past, clear the future commits
        if ( commandCount - commandIdx > 1 ) {
            const i32 commandToRemoveCount = ( commandCount - 1 );

            for ( i32 i = commandIdx; i < commandToRemoveCount; i++ ) {
                dk::core::free( cmdAllocator, commands[i] );
#ifdef DUSK_DEVBUILD
                commands[i] = nullptr;
#endif
            }

            commandCount -= ( commandToRemoveCount - commandIdx );
        }

        T* cmd = dk::core::allocate<T>( cmdAllocator, std::forward<TArgs>( args )... );
        cmd->execute();

        commands[commandIdx] = cmd;
    }

    void                            undo();
    void                            redo();

    bool                            canUndo() const;
	bool                            canRedo() const;

    const char*                     getPreviousActionName() const;
    const char*                     getNextActionName() const;

private:
    static constexpr i32            MAX_HISTORY_SIZE = 4096;

private:
    BaseAllocator*                  memoryAllocator;

    // Allocator responsible for the commands array allocation.
    PoolAllocator*                  cmdAllocator;

    // Array for commands buffering.
    TransactionCommand*             commands[MAX_HISTORY_SIZE];

    // Index in the commands array pointing to the latest command executed.
    i32                             commandIdx;

    // Number of command buffered in the commands array.
    i32                             commandCount;
};
