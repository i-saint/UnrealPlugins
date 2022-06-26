// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "MeshSyncStyle.h"

class FMeshSyncCommands : public TCommands<FMeshSyncCommands>
{
public:

	FMeshSyncCommands()
		: TCommands<FMeshSyncCommands>(TEXT("MeshSync"), NSLOCTEXT("Contexts", "MeshSync", "MeshSync Plugin"), NAME_None, FMeshSyncStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
};
