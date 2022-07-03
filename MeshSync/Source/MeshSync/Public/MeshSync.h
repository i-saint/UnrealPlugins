// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Engine/Selection.h"
#include "EngineUtils.h"


class FToolBarBuilder;
class FMenuBuilder;

class FMeshSyncModule : public IModuleInterface
{
public:

    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    
    /** This function will be bound to Command. */
    void CreateMeshAsset();
    void TestCreateActor();
    
private:

    void RegisterMenus();


private:
    TSharedPtr<class FUICommandList> PluginCommands;
};



AActor* GetParentActor(AActor* Child);
AActor* FindChildActor(AActor* Parent, const TCHAR* ChildLabel);
AActor* GetActorByPath(const FString& Path, AActor* Root = nullptr);

AActor* CreateChildActor(AActor* Parent, const TCHAR* ChildLabel);
AActor* GetOrCreateActorByPath(const FString& Path, AActor* Root = nullptr);

template<class Func>
inline int EachChildActor(AActor* Actor, Func&& f)
{
    if (!Actor)
        return 0;
    for (auto& Child : Actor->Children)
        f(Child);
    return Actor->Children.Num();
}

template<class ActorType = AActor>
inline ActorType* FindActorByName(UWorld* World, const TCHAR* Name)
{
    auto It = TActorIterator<ActorType>(World);
    while (It) {
        if ((*It)->GetActorLabel() == Name) {
            return *It;
        }
        ++It;
    }
    return nullptr;
}
