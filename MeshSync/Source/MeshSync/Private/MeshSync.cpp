// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshSync.h"
#include "MeshSyncStyle.h"
#include "MeshSyncCommands.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"

#include "RawMesh.h"

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
        FExecuteAction::CreateRaw(this, &FMeshSyncModule::PluginButtonClicked),
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

    // Object Details
    FString ObjectName = FString("MyObject");

    TArray<FVector3f> Vertices;
    Vertices.Emplace(86.6, 75, 0);
    Vertices.Emplace(-86.6, 75, 0);
    Vertices.Emplace(2.13, 25, 175);
    Vertices.Emplace(2.13, -75, 0);
    int numberOfVertices = Vertices.Num();

    struct Face {
        uint32 v1, v2, v3;
        int32 materialID;
        FVector2f uv1, uv2, uv3;
    };
    TArray<Face> Faces;
    Face oneFace;
    oneFace = { 1,3,0,  0,  {0,0}, {1,0}, {0.5, 1} };
    Faces.Add(oneFace);
    oneFace = { 0,2,1,  1,  {0,0}, {1,0}, {0.5, 1} };
    Faces.Add(oneFace);
    oneFace = { 3,2,0,  0,  {0,0}, {1,0}, {0.5, 1} };
    Faces.Add(oneFace);
    oneFace = { 1,2,3,  1,  {0,0}, {1,0}, {0.5, 1} };
    Faces.Add(oneFace);
    int numberOfFaces = Faces.Num();

    TArray<FStaticMaterial> Materials; //This should contain the real Materials, this is just an example
    Materials.Add(FStaticMaterial());
    Materials.Add(FStaticMaterial());
    int numberOfMaterials = Materials.Num();

    // Create Package
    FString pathPackage = FString("/Game/MeshSyncAssets/");
    FString absolutePathPackage = FPaths::ProjectContentDir() + "/MeshSyncAssets/";
    FPackageName::RegisterMountPoint(*pathPackage, *absolutePathPackage);
    UPackage* Package = CreatePackage(*pathPackage);

    // Create Static Mesh
    FName StaticMeshName = MakeUniqueObjectName(Package, UStaticMesh::StaticClass(), FName(*ObjectName));
    UStaticMesh* myStaticMesh = NewObject<UStaticMesh>(Package, StaticMeshName, RF_Public | RF_Standalone);

    if (myStaticMesh != NULL)
    {
        FRawMesh myRawMesh;
        FColor WhiteVertex = FColor(255, 255, 255, 255);
        FVector3f Zero3{ 0, 0, 0 };

        // Vertices
        for (int vertIndex = 0; vertIndex < numberOfVertices; vertIndex++) {
            myRawMesh.VertexPositions.Add(Vertices[vertIndex]);
        }
        // Faces and UV/Normals
        for (int faceIndex = 0; faceIndex < numberOfFaces; faceIndex++) {
            myRawMesh.WedgeIndices.Add(Faces[faceIndex].v1);
            myRawMesh.WedgeIndices.Add(Faces[faceIndex].v2);
            myRawMesh.WedgeIndices.Add(Faces[faceIndex].v3);

            myRawMesh.WedgeColors.Add(WhiteVertex);
            myRawMesh.WedgeColors.Add(WhiteVertex);
            myRawMesh.WedgeColors.Add(WhiteVertex);

            myRawMesh.WedgeTangentX.Add(Zero3);
            myRawMesh.WedgeTangentX.Add(Zero3);
            myRawMesh.WedgeTangentX.Add(Zero3);

            myRawMesh.WedgeTangentY.Add(Zero3);
            myRawMesh.WedgeTangentY.Add(Zero3);
            myRawMesh.WedgeTangentY.Add(Zero3);

            myRawMesh.WedgeTangentZ.Add(Zero3);
            myRawMesh.WedgeTangentZ.Add(Zero3);
            myRawMesh.WedgeTangentZ.Add(Zero3);

            // Materials
            myRawMesh.FaceMaterialIndices.Add(Faces[faceIndex].materialID);

            myRawMesh.FaceSmoothingMasks.Add(0xFFFFFFFF); // Phong

            for (int UVIndex = 0; UVIndex < MAX_MESH_TEXTURE_COORDS; UVIndex++)
            {
                myRawMesh.WedgeTexCoords[UVIndex].Add(Faces[faceIndex].uv1);
                myRawMesh.WedgeTexCoords[UVIndex].Add(Faces[faceIndex].uv2);
                myRawMesh.WedgeTexCoords[UVIndex].Add(Faces[faceIndex].uv3);
            }
        }

        // Saving mesh in the StaticMesh
        myStaticMesh->SetNumSourceModels(1);
        auto& SrcModel = myStaticMesh->GetSourceModel(0);
        SrcModel.RawMeshBulkData->SaveRawMesh(myRawMesh);

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

        for (int32 MaterialID = 0; MaterialID < numberOfMaterials; MaterialID++) {
            myStaticMesh->GetStaticMaterials().Add(Materials[MaterialID]);
            myStaticMesh->GetSectionInfoMap().Set(0, MaterialID, FMeshSectionInfo(MaterialID));
        }

        // Processing the StaticMesh and Marking it as not saved
        myStaticMesh->Build(false);

        myStaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;
        myStaticMesh->CreateBodySetup();
        myStaticMesh->SetLightingGuid();
        myStaticMesh->PostEditChange();
        Package->MarkPackageDirty();

        UE_LOG(LogTemp, Log, TEXT("Static Mesh created: %s"), &ObjectName);
    }
}

void FMeshSyncModule::PluginButtonClicked()
{
    //// Put your "OnButtonClicked" stuff here
    //FText DialogText = FText::Format(
    //    LOCTEXT("PluginButtonDialogText", "Add code to {0} in {1} to override this button's actions"),
    //    FText::FromString(TEXT("FMeshSyncModule::PluginButtonClicked()")),
    //    FText::FromString(TEXT("MeshSync.cpp"))
    //);
    //FMessageDialog::Open(EAppMsgType::Ok, DialogText);

    CreateStaticMesh();
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FMeshSyncModule, MeshSync)