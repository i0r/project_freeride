/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "TransactionHandler.h"

static std::string EMPTY_COMMAND = "";

TransactionHandler::TransactionHandler( BaseAllocator* allocator )
    : cmdAllocator( allocator )
    , commandIdx( -1 )
    , commandCount( 0 )
    , commands{ nullptr }
{

}

TransactionHandler::~TransactionHandler()
{
    for ( int i = 0; i < commandCount; i++ ) {
        dk::core::free( cmdAllocator, commands[i] );
    }
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
    if ( ( commandIdx + 1 ) >= commandCount ) {
        return;
    }

    commands[commandIdx + 1]->execute();
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
