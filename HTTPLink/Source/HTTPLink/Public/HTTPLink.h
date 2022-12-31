#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Async/Future.h"
#include "Containers/Ticker.h"

#include "HttpServerModule.h"
#include "HttpRouteHandle.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"


class HTTPLINK_API FHTTPLinkModule
    : public IModuleInterface
    , public FTSTickerObjectBase
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
    virtual bool Tick(float DeltaTime) override;

    TSharedRef<FExtender> BuildActorContextMenu(const TSharedRef<FUICommandList> CommandList, const TArray<AActor*> Actors);
    void CopyLinkAddress(const TArray<AActor*> Actors);

    // editor commands
    bool OnEditorExec(const FHttpServerRequest& Request, const FHttpResultCallback& Result);
    bool OnEditorScreenshot(const FHttpServerRequest& Request, const FHttpResultCallback& Result);
    void OnScreenshotProcessed();

    // actor commands
    bool OnActorList(const FHttpServerRequest& Request, const FHttpResultCallback& Result);
    bool OnActorSelect(const FHttpServerRequest& Request, const FHttpResultCallback& Result);
    bool OnActorFocus(const FHttpServerRequest& Request, const FHttpResultCallback& Result);
    bool OnActorCreate(const FHttpServerRequest& Request, const FHttpResultCallback& Result);
    bool OnActorDelete(const FHttpServerRequest& Request, const FHttpResultCallback& Result);
    bool OnActorMerge(const FHttpServerRequest& Request, const FHttpResultCallback& Result);

    // level commands
    bool OnLevelNew(const FHttpServerRequest& Request, const FHttpResultCallback& Result);
    bool OnLevelLoad(const FHttpServerRequest& Request, const FHttpResultCallback& Result);
    bool OnLevelSave(const FHttpServerRequest& Request, const FHttpResultCallback& Result);

    // asset commands
    bool OnAssetList(const FHttpServerRequest& Request, const FHttpResultCallback& Result);
    bool OnAssetImport(const FHttpServerRequest& Request, const FHttpResultCallback& Result);

    // test commands
    bool OnTest(const FHttpServerRequest& Request, const FHttpResultCallback& Result);

private:
    FPlatformProcess::FSemaphore* GlobalLock = nullptr;
    TSharedPtr<IHttpRouter> Router;
    TArray<FHttpRouteHandle> HRoutes;
    FSimpleOutputDevice Outputs;

    FDelegateHandle HScreenshot;
    bool bScreenshotInProgress = false;
};
