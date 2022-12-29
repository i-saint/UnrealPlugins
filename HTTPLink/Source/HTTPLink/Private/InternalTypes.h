#pragma once
#include "InternalTypes.generated.h"


USTRUCT()
struct FActorComponentSummary
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY()
    FString TypeName;
        
    UPROPERTY()
    FName Name;

    FActorComponentSummary(UActorComponent* Component = nullptr);
    void Setup(UActorComponent* Component);
};


USTRUCT()
struct FActorSummary
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY()
    FString TypeName;

    UPROPERTY()
    FString Label;
    
    UPROPERTY()
    FName Name;

    UPROPERTY()
    FGuid GUID;

    UPROPERTY()
    FTransform Transform;

    UPROPERTY()
    TArray<FActorComponentSummary> Components;


    FActorSummary(AActor* Actor = nullptr);
    void Setup(AActor* Actor);
    TSharedPtr<FJsonObject> ToJson() const;
};


USTRUCT()
struct FAssetSummary
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY()
    FString TypeName;

    UPROPERTY()
    FName AssetName;

    UPROPERTY()
    FName PackageName;

    UPROPERTY()
    FString ObjectPath;


    FAssetSummary();
    FAssetSummary(const FAssetData& Data);
    void Setup(const FAssetData& Data);
    TSharedPtr<FJsonObject> ToJson() const;
};

