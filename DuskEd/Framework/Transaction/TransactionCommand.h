/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include <string>

class TransactionCommand
{
public:
    virtual void execute() = 0;
    virtual void undo() = 0;

    const std::string& getActionInfos() const 
    { 
        return actionInfos; 
    }

protected:
    std::string  actionInfos;
};
