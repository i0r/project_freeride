/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "TransactionHandler.h"

#include "Core/Allocators/PoolAllocator.h"

static std::string EMPTY_COMMAND = "";

TransactionHandler::TransactionHandler( BaseAllocator* allocator )
    : memoryAllocator( allocator )
    , cmdAllocator( dk::core::allocate<PoolAllocator>( allocator, sizeof( TransactionCommand ), static_cast<u8>( alignof( TransactionCommand ) ), sizeof( TransactionCommand ) * MAX_HISTORY_SIZE, allocator->allocate( sizeof( TransactionCommand )* MAX_HISTORY_SIZE ) ) )
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
    if ( commandIdx < 0 ) {
        return;
    }

    commands[commandIdx]->undo();
    commandIdx--;
}

void TransactionHandler::redo()
{
    i32 nextCmdIdx = ( commandIdx + 1 );
    if ( nextCmdIdx >= commandCount ) {
        return;
    }

    commands[nextCmdIdx]->execute();
    commandIdx++;
}


const std::string& TransactionHandler::getPreviousActionName() const
{
    return ( commandIdx < 0 ) ? EMPTY_COMMAND : commands[commandIdx]->getActionInfos();
}

const std::string& TransactionHandler::getNextActionName() const
{
    return ( commandIdx + 1 ) >= commandCount ? EMPTY_COMMAND : commands[commandIdx + 1]->getActionInfos();
}
