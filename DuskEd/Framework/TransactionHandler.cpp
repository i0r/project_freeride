/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "TransactionHandler.h"

#include "Core/Allocators/PoolAllocator.h"

static constexpr i32 MAX_TRANSACTION_CMD_SIZE = 256;

TransactionHandler::TransactionHandler( BaseAllocator* allocator )
    : memoryAllocator( allocator )
    , cmdAllocator( dk::core::allocate<PoolAllocator>( 
            allocator, 
            MAX_TRANSACTION_CMD_SIZE, 
            static_cast<u8>( alignof( TransactionCommand ) ), 
            MAX_TRANSACTION_CMD_SIZE * MAX_HISTORY_SIZE, 
            allocator->allocate( MAX_TRANSACTION_CMD_SIZE * MAX_HISTORY_SIZE ) 
        ) )
    , commandIdx( -1 )
    , commandCount( 0 )
    , commands{ nullptr }
{

}

TransactionHandler::~TransactionHandler()
{
    dk::core::free( memoryAllocator, cmdAllocator );
}

void TransactionHandler::undo()
{
    if ( !canUndo() ) {
        return;
    }

    commands[commandIdx]->undo();
    commandIdx--;
}

void TransactionHandler::redo()
{
    if ( !canRedo() ) {
        return;
    }

	commandIdx++;
    commands[commandIdx]->execute();
}

bool TransactionHandler::canUndo() const
{
    return ( commandIdx >= 0 );
}

bool TransactionHandler::canRedo() const
{
	i32 nextCmdIdx = ( commandIdx + 1 );
    return ( nextCmdIdx < commandCount );
}

const char* TransactionHandler::getPreviousActionName() const
{
    return ( !canUndo() ) ? nullptr : commands[commandIdx]->getActionInfos().c_str();
}

const char* TransactionHandler::getNextActionName() const
{
    return ( !canRedo() ) ? nullptr : commands[commandIdx + 1]->getActionInfos().c_str();
}
