#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "GenericPlatform/GenericPlatformProcess.h"

#include "HttpServerModule.h"
#include "HttpRouteHandle.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"

#include "HTTPLink.generated.h"


USTRUCT()
struct HTTPLINK_API FActorSummary
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY()
    FString ActorLabel;

    UPROPERTY()
    FGuid ActorGUID;

    UPROPERTY()
    FString TypeName;


    FActorSummary(AActor* Actor = nullptr);
    void Setup(AActor* Actor);
};

USTRUCT()
struct HTTPLINK_API FActorSummaryArray
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY()
    TArray<FActorSummary> Data;

    void Add(AActor* Actor);
    FString ToJson() const;
};



class HTTPLINK_API FHTTPLinkModule : public IModuleInterface
{
public:
    class FSimpleOutputDevice : public FOutputDevice
    {
    using Super = FOutputDevice;
    public:
        FSimpleOutputDevice();
        void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override;
        void Clear();

        FString Log;
    };



public:
    const int PORT = 8110;

    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    bool Respond(const FHttpResultCallback& Result, const FString& Content = {});
    bool RespondJson(const FHttpResultCallback& Result, const FString& Content);
    bool OnExec(const FHttpServerRequest& Request, const FHttpResultCallback& Result);
    bool OnListActor(const FHttpServerRequest& Request, const FHttpResultCallback& Result);
    bool OnSelectActor(const FHttpServerRequest& Request, const FHttpResultCallback& Result);
    bool OnFocusActor(const FHttpServerRequest& Request, const FHttpResultCallback& Result);
    bool OnCreateActor(const FHttpServerRequest& Request, const FHttpResultCallback& Result);
    bool OnNewLevel(const FHttpServerRequest& Request, const FHttpResultCallback& Result);
    bool OnLoadLevel(const FHttpServerRequest& Request, const FHttpResultCallback& Result);
    bool OnSaveLevel(const FHttpServerRequest& Request, const FHttpResultCallback& Result);

    TSharedRef<FExtender> BuildContextMenu(const TSharedRef<FUICommandList> CommandList, const TArray<AActor*> Actors);
    void CopyLinkAddress(const TArray<AActor*> Actors);

private:
    FSimpleOutputDevice Outputs;
    FPlatformProcess::FSemaphore* GlobalLock = nullptr;
    TSharedPtr<IHttpRouter> Router;
    TArray<FHttpRouteHandle> HRoutes;
};
