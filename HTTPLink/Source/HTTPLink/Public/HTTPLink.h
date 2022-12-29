#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "GenericPlatform/GenericPlatformProcess.h"

#include "HttpServerModule.h"
#include "HttpRouteHandle.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"


class HTTPLINK_API FSimpleOutputDevice : public FOutputDevice
{
using Super = FOutputDevice;
public:
    FSimpleOutputDevice();
    void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override;
    void Clear();

    FString Log;
};

class HTTPLINK_API FHTTPLinkModule : public IModuleInterface
{
public:
    const int PORT = 8110;

    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    bool Respond(const FHttpResultCallback& Result, const FString& Content = {});
    bool OnFocus(const FHttpServerRequest& Request, const FHttpResultCallback& Result);
    bool OnExec(const FHttpServerRequest& Request, const FHttpResultCallback& Result);

    TSharedRef<FExtender> BuildContextMenu(const TSharedRef<FUICommandList> CommandList, const TArray<AActor*> Actors);
    void CopyLinkAddress(const TArray<AActor*> Actors);

private:
    FSimpleOutputDevice Outputs;
    FPlatformProcess::FSemaphore* GlobalLock = nullptr;
    TSharedPtr<IHttpRouter> Router;
    TArray<FHttpRouteHandle> HRoutes;
};
