#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#include "HttpServerModule.h"
#include "HttpRouteHandle.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"

class FHTTPLinkModule : public IModuleInterface
{
public:
	const int PORT = 8110;

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	bool Respond(const FHttpResultCallback& Result, FString&& Content = {});
	bool Focus(const FHttpServerRequest& Request, const FHttpResultCallback& Result);

	TSharedRef<FExtender> BuildContextMenu(const TSharedRef<FUICommandList> CommandList, const TArray<AActor*> Actors);
	void CopyLinkAddress(const TArray<AActor*> Actors);

private:
	TSharedPtr<IHttpRouter> m_Router;
	FHttpRouteHandle m_HFocus;
};
