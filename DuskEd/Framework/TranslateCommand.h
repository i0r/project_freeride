/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include "TransactionCommand.h"

class TranslateCommand : public TransactionCommand
{
public:
    TranslateCommand( TransformDatabase::EdInstanceData* transformToEdit, const dkVec3f& backupTranslationValue, const dkVec3f& translationValue )
        : transform( transformToEdit )
        , translation( translationValue )
        , translationBackup( backupTranslationValue )
    {
        actionInfos = "Apply Translation";
    }

    virtual void execute() override
    {
        *transform->Position = translation;
    }

    virtual void undo() override
    {
        *transform->Position = translationBackup;
    }

private:
    TransformDatabase::EdInstanceData*  transform;
    dkVec3f                             translation;
    dkVec3f                             translationBackup;
};
