// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshSyncCommands.h"

#define LOCTEXT_NAMESPACE "FMeshSyncModule"

void FMeshSyncCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "Test Command", "Test command", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
