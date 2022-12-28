// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshSync.h"
#include "MeshSyncStyle.h"
#include "MeshSyncCommands.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"
#include "UnrealEdGlobals.h"
#include "RawMesh.h"
#include "ScopedTransaction.h"

static const FName MeshSyncTabName("MeshSync");

#define LOCTEXT_NAMESPACE "FMeshSyncModule"

void FMeshSyncModule::StartupModule()
{
    // This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
    
    FMeshSyncStyle::Initialize();
    FMeshSyncStyle::ReloadTextures();

    FMeshSyncCommands::Register();
    
    PluginCommands = MakeShareable(new FUICommandList);

    PluginCommands->MapAction(
        FMeshSyncCommands::Get().PluginAction,
        FExecuteAction::CreateRaw(this, &FMeshSyncModule::CreateMeshAsset),
        FCanExecuteAction());

    UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FMeshSyncModule::RegisterMenus));
}

void FMeshSyncModule::ShutdownModule()
{
    // This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
    // we call this function before unloading the module.

    UToolMenus::UnRegisterStartupCallback(this);

    UToolMenus::UnregisterOwner(this);

    FMeshSyncStyle::Shutdown();

    FMeshSyncCommands::Unregister();
}

void FMeshSyncModule::RegisterMenus()
{
    // Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
    FToolMenuOwnerScoped OwnerScoped(this);

    {
        UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
        {
            FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
            Section.AddMenuEntryWithCommandList(FMeshSyncCommands::Get().PluginAction, PluginCommands);
        }
    }

    {
        UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
        {
            FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
            {
                FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FMeshSyncCommands::Get().PluginAction));
                Entry.SetCommandList(PluginCommands);
            }
        }
    }
}


static void CreateStaticMesh()
{
    auto UndoScope = FScopedTransaction(LOCTEXT("CreateStaticMesh", "CreateStaticMesh"));

    // Object Details
    FString ObjectName = FString("MyObject");

    TArray<FVector3f> Vertices;
    Vertices.Emplace(86.6, 75, 0);
    Vertices.Emplace(-86.6, 75, 0);
    Vertices.Emplace(2.13, 25, 175);
    Vertices.Emplace(2.13, -75, 0);
    int NumVertices = Vertices.Num();

    struct Face {
        uint32 V1, V2, V3;
        int32 MaterialID;
        FVector2f UV1, UV2, UV3;
    };
    TArray<Face> Faces;
    Faces.Add({ 1,3,0, 0, {0,0}, {1,0}, {0.5, 1} });
    Faces.Add({ 0,2,1, 1, {0,0}, {1,0}, {0.5, 1} });
    Faces.Add({ 3,2,0, 0, {0,0}, {1,0}, {0.5, 1} });
    Faces.Add({ 1,2,3, 1, {0,0}, {1,0}, {0.5, 1} });
    int NumFaces = Faces.Num();

    TArray<FStaticMaterial> Materials; //This should contain the real Materials, this is just an example
    Materials.Add(FStaticMaterial());
    Materials.Add(FStaticMaterial());
    int NumMaterials = Materials.Num();

    // Create Package
    FString PackagePath = FString("/Game/MeshSyncAssets/");
    FString AbsPackagePath = FPaths::ProjectContentDir() + "/MeshSyncAssets/";
    FPackageName::RegisterMountPoint(*PackagePath, *AbsPackagePath);
    UPackage* Package = CreatePackage(*PackagePath);

    // Create Static Mesh
    FName StaticMeshName = MakeUniqueObjectName(Package, UStaticMesh::StaticClass(), FName(*ObjectName));
    UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Package, StaticMeshName, RF_Public | RF_Standalone | RF_Transactional);

    if (StaticMesh) {
        FRawMesh RawMesh;
        FColor WhiteVertex = FColor(255, 255, 255, 255);
        FVector3f Zero3{ 0, 0, 0 };

        // Vertices
        for (int VI = 0; VI < NumVertices; VI++) {
            RawMesh.VertexPositions.Add(Vertices[VI]);
        }
        // Faces and UV/Normals
        for (int FI = 0; FI < NumFaces; FI++) {
            RawMesh.WedgeIndices.Add(Faces[FI].V1);
            RawMesh.WedgeIndices.Add(Faces[FI].V2);
            RawMesh.WedgeIndices.Add(Faces[FI].V3);

            RawMesh.WedgeColors.Add(WhiteVertex);
            RawMesh.WedgeColors.Add(WhiteVertex);
            RawMesh.WedgeColors.Add(WhiteVertex);

            RawMesh.WedgeTangentX.Add(Zero3);
            RawMesh.WedgeTangentX.Add(Zero3);
            RawMesh.WedgeTangentX.Add(Zero3);

            RawMesh.WedgeTangentY.Add(Zero3);
            RawMesh.WedgeTangentY.Add(Zero3);
            RawMesh.WedgeTangentY.Add(Zero3);

            RawMesh.WedgeTangentZ.Add(Zero3);
            RawMesh.WedgeTangentZ.Add(Zero3);
            RawMesh.WedgeTangentZ.Add(Zero3);

            // Materials
            RawMesh.FaceMaterialIndices.Add(Faces[FI].MaterialID);

            RawMesh.FaceSmoothingMasks.Add(0xFFFFFFFF); // Phong

            for (int UVIndex = 0; UVIndex < MAX_MESH_TEXTURE_COORDS; UVIndex++) {
                RawMesh.WedgeTexCoords[UVIndex].Add(Faces[FI].UV1);
                RawMesh.WedgeTexCoords[UVIndex].Add(Faces[FI].UV2);
                RawMesh.WedgeTexCoords[UVIndex].Add(Faces[FI].UV3);
            }
        }

        // Saving mesh in the StaticMesh
        StaticMesh->SetNumSourceModels(1);
        auto& SrcModel = StaticMesh->GetSourceModel(0);
        SrcModel.RawMeshBulkData->SaveRawMesh(RawMesh);

        // Model Configuration
        SrcModel.BuildSettings.bUseMikkTSpace = false;
        SrcModel.BuildSettings.bRecomputeNormals = false;
        SrcModel.BuildSettings.bRecomputeTangents = false;
        SrcModel.BuildSettings.bRemoveDegenerates = false;
        SrcModel.BuildSettings.bUseHighPrecisionTangentBasis = false;

        SrcModel.BuildSettings.bGenerateLightmapUVs = true;
        SrcModel.BuildSettings.bBuildReversedIndexBuffer = false;
        SrcModel.BuildSettings.bUseFullPrecisionUVs = false;

        // Assign the Materials to the Slots (optional

        for (int32 MaterialID = 0; MaterialID < NumMaterials; MaterialID++) {
            StaticMesh->GetStaticMaterials().Add(Materials[MaterialID]);
            StaticMesh->GetSectionInfoMap().Set(0, MaterialID, FMeshSectionInfo(MaterialID));
        }

        // Processing the StaticMesh and Marking it as not saved
        StaticMesh->Build(false);

        StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;
        StaticMesh->CreateBodySetup();
        StaticMesh->SetLightingGuid();
        StaticMesh->PostEditChange();
        Package->MarkPackageDirty();

        UE_LOG(LogTemp, Log, TEXT("Static Mesh created: %s"), &ObjectName);
    }
}

void FMeshSyncModule::CreateMeshAsset()
{
    CreateStaticMesh();
    //TestCreateActor();
}

void FMeshSyncModule::TestCreateActor()
{
    AActor* Root = nullptr;
    if (auto* Selection = GEditor->GetSelectedActors()) {
        for (FSelectionIterator It(*Selection); It; ++It) {
            Root = Cast<AActor>(*It);
            if (Root) {
                break;
            }
        }
    }

    GetOrCreateActorByPath("/This/Is/A/Test", Root);
}



#pragma region Utils

AActor* GetParentActor(AActor* Child)
{
    return Child->GetRootComponent()->GetAttachParent()->GetOwner();
}

AActor* FindChildActor(AActor* Parent, const TCHAR* ChildName)
{
    AActor* Ret = nullptr;
    EachChildActor(Parent, [&Ret, ChildName](AActor* Child) {
        if (Child->GetActorLabel() == ChildName) {
            Ret = Child;
        }
        });
    return Ret;
}

AActor* GetActorByPath(const FString& Path, AActor* Root)
{
    TArray<FString> ChildNames;
    Path.ParseIntoArray(ChildNames, TEXT("/"), false);
    if (ChildNames.Num() == 0) {
        return nullptr;
    }
    else if (ChildNames[0].Len() == 0) {
        ChildNames.RemoveAt(0);
    }

    AActor* Ret = nullptr;

    for (int i = 0; i < ChildNames.Num(); ++i) {
        if (i == 0) {
            if (Root) {
                Ret = FindChildActor(Root, *ChildNames[i]);
            }
            else{
                auto* World = GEditor->GetEditorWorldContext().World();
                Ret = FindActorByName(World, *ChildNames[i]);
            }
        }
        else {
            Ret = FindChildActor(Ret, *ChildNames[i]);

        }

        if (!Ret) {
            return nullptr;
        }
    }
    return Ret;

}

AActor* CreateChildActor(AActor* Parent, const TCHAR* ChildName)
{
    FActorSpawnParameters Params;
    Params.Owner = Parent;
    Params.Name = FName(ChildName);
    Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

    auto World = Parent ? Parent->GetWorld() : GEditor->GetEditorWorldContext().World();
    auto Ret = World->SpawnActor<AActor>(Params);
    if (Ret) {
        Ret->SetActorLabel(ChildName);
    }
    return Ret;
}

AActor* GetOrCreateActorByPath(const FString& Path, AActor* Root)
{
    TArray<FString> ChildNames;
    Path.ParseIntoArray(ChildNames, TEXT("/"), false);
    if (ChildNames.Num() == 0) {
        return nullptr;
    }
    else if (ChildNames[0].Len() == 0) {
        ChildNames.RemoveAt(0);
    }

    AActor* Ret = nullptr;

    for (int i = 0; i < ChildNames.Num(); ++i) {
        if (i == 0) {
            if (Root) {
                Ret = FindChildActor(Root, *ChildNames[i]);
                if (!Ret) {
                    Ret = CreateChildActor(Root, *ChildNames[i]);
                }
            }
            else {
                auto* World = GEditor->GetEditorWorldContext().World();
                Ret = FindActorByName(World, *ChildNames[i]);
                if (!Ret) {
                    Ret = CreateChildActor(nullptr, *ChildNames[i]);
                }
            }
        }
        else {
            auto Parent = Ret;
            Ret = FindChildActor(Parent, *ChildNames[i]);
            if (!Ret) {
                Ret = CreateChildActor(Parent, *ChildNames[i]);
            }
        }

        if (!Ret) {
            return nullptr;
        }
    }
    return Ret;

}

#pragma endregion Utils


#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FMeshSyncModule, MeshSync)