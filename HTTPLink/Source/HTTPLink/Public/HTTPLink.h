#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "GenericPlatform/GenericPlatformProcess.h"

#include "HttpServerModule.h"
#include "HttpRouteHandle.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"


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

    TSharedRef<FExtender> BuildContextMenu(const TSharedRef<FUICommandList> CommandList, const TArray<AActor*> Actors);
    void CopyLinkAddress(const TArray<AActor*> Actors);

    // editor commands
    bool OnExec(const FHttpServerRequest& Request, const FHttpResultCallback& Result);

    // actor commands
    bool OnListActor(const FHttpServerRequest& Request, const FHttpResultCallback& Result);
    bool OnSelectActor(const FHttpServerRequest& Request, const FHttpResultCallback& Result);
    bool OnFocusActor(const FHttpServerRequest& Request, const FHttpResultCallback& Result);
    bool OnCreateActor(const FHttpServerRequest& Request, const FHttpResultCallback& Result);
    bool OnDeleteActor(const FHttpServerRequest& Request, const FHttpResultCallback& Result);
    bool OnMergeActor(const FHttpServerRequest& Request, const FHttpResultCallback& Result);

    // level commands
    bool OnNewLevel(const FHttpServerRequest& Request, const FHttpResultCallback& Result);
    bool OnLoadLevel(const FHttpServerRequest& Request, const FHttpResultCallback& Result);
    bool OnSaveLevel(const FHttpServerRequest& Request, const FHttpResultCallback& Result);

    // asset commands
    bool OnListAsset(const FHttpServerRequest& Request, const FHttpResultCallback& Result);
    bool OnImportAsset(const FHttpServerRequest& Request, const FHttpResultCallback& Result);

private:
    FSimpleOutputDevice Outputs;
    FPlatformProcess::FSemaphore* GlobalLock = nullptr;
    TSharedPtr<IHttpRouter> Router;
    TArray<FHttpRouteHandle> HRoutes;
};
