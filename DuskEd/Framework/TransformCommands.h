/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include "TransactionCommand.h"

class TranslateCommand : public TransactionCommand
{
public:
    TranslateCommand( TransformDatabase::EdInstanceData* transformToEdit, const dkVec3f& newValue, const dkVec3f& oldValue )
        : translationToEdit( transformToEdit->Position )
        , translation( newValue )
        , previousTranslation( oldValue )
    {
        actionInfos = "Apply Translation";
    }

    virtual void execute() override
    {
		*translationToEdit = translation;
    }

    virtual void undo() override
    {
        *translationToEdit = previousTranslation;
    }

private:
    dkVec3f*	translationToEdit;

    dkVec3f		translation;
    dkVec3f		previousTranslation;
};

class RotateCommand : public TransactionCommand
{
public:
	RotateCommand( TransformDatabase::EdInstanceData* transformToEdit, const dkQuatf& newValue, const dkQuatf& oldValue )
		: rotationToEdit( transformToEdit->Rotation )
		, rotation( newValue )
		, previousRotation( oldValue )
	{
		actionInfos = "Apply Rotation";
	}

	virtual void execute() override
	{
		*rotationToEdit = rotation;
	}

	virtual void undo() override
	{
		*rotationToEdit = previousRotation;
	}

private:
	dkQuatf*	rotationToEdit;
	dkQuatf		rotation;
	dkQuatf     previousRotation;
};

class ScaleCommand : public TransactionCommand
{
public:
	ScaleCommand( TransformDatabase::EdInstanceData* transformToEdit, const dkVec3f& newValue, const dkVec3f& oldValue )
		: scaleToEdit( transformToEdit->Scale )
		, scale( newValue )
		, previousScale( oldValue )
	{
		actionInfos = "Apply Scale";
	}

	virtual void execute() override
	{
		*scaleToEdit = scale;
	}

	virtual void undo() override
	{
		*scaleToEdit = previousScale;
	}

private:
	dkVec3f*	scaleToEdit;
	dkVec3f     scale;
	dkVec3f     previousScale;
};