#include "HTTPLink.h"
#include "./InternalTypes.h"

#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "EngineUtils.h"
#include "LevelEditor.h"
#include "LevelEditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/PlatformApplicationMisc.h"
#include "GenericPlatform/GenericPlatformApplicationMisc.h"
#include "Misc/Guid.h"
#include "AssetRegistry/AssetData.h"
#include "ScopedTransaction.h"
#include "Misc/FileHelper.h"
#include "JsonObjectConverter.h"


#if PLATFORM_WINDOWS
#include "IDesktopPlatform.h"
#include "Developer/DesktopPlatform/Private/Windows/DesktopPlatformWindows.h"
#endif

#define LOCTEXT_NAMESPACE "FHTTPLinkModule"

#pragma region Utilities
static inline UWorld* GetEditorWorld()
{
    return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

template<class ActorType = AActor, class Func>
inline size_t EachActor(UWorld* World, Func&& F)
{
    size_t Ret = 0;
    if (World) {
        for (auto It = TActorIterator<ActorType>(World); It; ++It) {
            F(*It);
            ++Ret;
        }
    }
    return Ret;
}

template<class ActorType = AActor, class Cond>
inline ActorType* FindActor(UWorld* World, Cond&& C)
{
    if (World) {
        for (auto It = TActorIterator<ActorType>(World); It; ++It) {
            if (C(*It))
                return *It;
        }
    }
    return nullptr;
}

template<class T>
inline bool GetQueryParam(const FHttpServerRequest& Request, const char* Name, T& Dst);
// bool
template<>
inline bool GetQueryParam(const FHttpServerRequest& Request, const char* Name, bool& Dst)
{
    if (auto* V = Request.QueryParams.Find(Name)) {
        Dst = V->ToBool();
    }
    return false;
}
// int64
template<>
inline bool GetQueryParam(const FHttpServerRequest& Request, const char* Name, int64& Dst)
{
    if (auto* V = Request.QueryParams.Find(Name)) {
        if (V->StartsWith("0x"))
            Dst = FParse::HexNumber64(**V);
        else
            Dst = FCString::Atoi64(**V);
        return true;
    }
    return false;
}
// FString
template<>
inline bool GetQueryParam(const FHttpServerRequest& Request, const char* Name, FString& Dst)
{
    if (auto* V = Request.QueryParams.Find(Name)) {
        Dst = *V;
        return true;
    }
    return false;
}
// FName
template<>
inline bool GetQueryParam(const FHttpServerRequest& Request, const char* Name, FName& Dst)
{
    if (auto* V = Request.QueryParams.Find(Name)) {
        Dst = FName(*V);
        return true;
    }
    return false;
}
// FGuid
template<>
inline bool GetQueryParam(const FHttpServerRequest& Request, const char* Name, FGuid& Dst)
{
    if (auto* V = Request.QueryParams.Find(Name)) {
        Dst = FGuid(*V);
        return true;
    }
    return false;
}
// FVector
template<>
inline bool GetQueryParam(const FHttpServerRequest& Request, const char* Name, FVector& Dst)
{
    if (auto* V = Request.QueryParams.Find(Name)) {
        float X, Y, Z;
        if (swscanf_s(**V, TEXT("%f,%f,%f"), &X, &Y, &Z) == 3) {
            Dst = FVector(X, Y, Z);
        }
        return true;
    }
    return false;
}


static void MakeEditorWindowForeground()
{
    auto EditorWindow = FSlateApplication::Get().FindBestParentWindowForDialogs(nullptr, ESlateParentWindowSearchMethod::MainWindow);
    if (EditorWindow) {
#if PLATFORM_WINDOWS
        // FWindowsWindow::HACK_ForceToFront() は Windows では SetForegroundWindow() なのだが、
        // 通常、バックグラウンドのプロセスから SetForegroundWindow() を呼んでもタスクバーのアイコンがフラッシュするだけで最前面にはならない。
        // ( https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setforegroundwindow#remarks )
        // これを回避するため、AttachThreadInput() で一時的にその時点での最前面ウィンドウの状態を間借りする。
        HWND HW = (HWND)EditorWindow->GetNativeWindow()->GetOSWindowHandle();
        DWORD FromTID = ::GetWindowThreadProcessId(::GetForegroundWindow(), nullptr);
        DWORD ToTID = ::GetWindowThreadProcessId(HW, nullptr);
        if (FromTID != ToTID) {
            ::AttachThreadInput(FromTID, ToTID, true);
            ::SetForegroundWindow(HW); // == EditorWindow->GetNativeWindow()->HACK_ForceToFront()
            ::AttachThreadInput(FromTID, ToTID, false);
        }
        else
        {
            ::SetForegroundWindow(HW);
        }
#else
        EditorWindow->GetNativeWindow()->HACK_ForceToFront();
#endif
    }
}

static void AddAccessControl(FHttpServerResponse& Response)
{
    Response.Headers.Add("Access-Control-Allow-Origin", { "*" });
}

static bool Respond(const FHttpResultCallback& Result, const FString& Content = {})
{
    // FHttpServerResponse::Ok() は 204 No Content を返すので使わない方がいい
    auto Response = FHttpServerResponse::Create(Content, "text/plain");
    Response->Code = EHttpServerResponseCodes::Ok;
    AddAccessControl(*Response);
    Result(MoveTemp(Response));
    return true;
}

static bool RespondJson(const FHttpResultCallback& Result, TSharedPtr<FJsonObject> Json)
{
    FString Content;
    {
        auto Writer = TJsonWriterFactory<>::Create(&Content);
        FJsonSerializer::Serialize(Json.ToSharedRef(), Writer);
    }
    auto Response = FHttpServerResponse::Create(Content, "application/json");
    Response->Code = EHttpServerResponseCodes::Ok;
    AddAccessControl(*Response);
    Result(MoveTemp(Response));
    return true;
}

static bool RespondJson(const FHttpResultCallback& Result, const TArray<TSharedPtr<FJsonValue>>& Json)
{
    FString Content;
    {
        auto Writer = TJsonWriterFactory<>::Create(&Content);
        FJsonSerializer::Serialize(Json, Writer);
    }
    auto Response = FHttpServerResponse::Create(Content, "application/json");
    Response->Code = EHttpServerResponseCodes::Ok;
    AddAccessControl(*Response);
    Result(MoveTemp(Response));
    return true;
}

static bool RespondRetry(const FHttpResultCallback& Result, FString Location, int Second)
{
    auto Response = FHttpServerResponse::Create(FString(), "text/plain");
    Response->Code = EHttpServerResponseCodes::Redirect;
    Response->Headers.Add("Location", { Location });
    Response->Headers.Add("Retry-After", { FString::Printf(TEXT("%d"), Second)});
    AddAccessControl(*Response);
    Result(MoveTemp(Response));
    return true;
}

static bool ServeFile(const FHttpResultCallback& Result, FString FilePath, FString ContentType)
{
    TArray<uint8> Data;
    bool Ok = FFileHelper::LoadFileToArray(Data, *FilePath);

    auto Response = FHttpServerResponse::Create(MoveTemp(Data), ContentType);
    Response->Code = Ok ? EHttpServerResponseCodes::Ok : EHttpServerResponseCodes::NotFound;
    AddAccessControl(*Response);
    Result(MoveTemp(Response));
    return true;
}


template<class T>
static bool MatchClass(const FAssetData& Asset)
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
    return Asset.AssetClassPath.GetAssetName() == T::StaticClass()->GetFName();
#else
    return Asset.AssetClass == T::StaticClass()->GetFName();
#endif
}

static FString GetObectPathStr(const FAssetData& Asset)
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
    return Asset.GetObjectPathString();
#else
    return Asset.ObjectPath.ToString();
#endif
}
#pragma endregion Utilities


#pragma region InternalTypes
FActorComponentSummary::FActorComponentSummary(UActorComponent* Component)
{
    Setup(Component);
}

void FActorComponentSummary::Setup(UActorComponent* Component)
{
    if (Component) {
        TypeName = Component->GetClass()->GetName();
        Name = Component->GetFName();
    }
}

FActorSummary::FActorSummary(AActor* Actor)
{
    Setup(Actor);
}

void FActorSummary::Setup(AActor* Actor)
{
    if (Actor) {
        TypeName = Actor->GetClass()->GetName();
        Label = Actor->GetActorLabel();
        Name = Actor->GetFName();
        GUID = Actor->GetActorGuid();
        Transform = Actor->GetActorTransform();

        for (auto Component : Actor->GetComponents()) {
            Components.Emplace(Component);
        }
    }
}

TSharedPtr<FJsonObject> FActorSummary::ToJson() const
{
    return FJsonObjectConverter::UStructToJsonObject(*this);
}


FAssetSummary::FAssetSummary()
{
}

FAssetSummary::FAssetSummary(const FAssetData& Data)
{
    Setup(Data);
}

void FAssetSummary::Setup(const FAssetData& Data)
{
    TypeName = Data.GetClass()->GetName();
    AssetName = Data.AssetName;
    ObjectPath = GetObectPathStr(Data);
    PackageName = Data.PackageName;
}

TSharedPtr<FJsonObject> FAssetSummary::ToJson() const
{
    return FJsonObjectConverter::UStructToJsonObject(*this);
}



FHTTPLinkModule::FSimpleOutputDevice::FSimpleOutputDevice()
    : Super()
{
}

void FHTTPLinkModule::FSimpleOutputDevice::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category)
{
    Log += V;
}

void FHTTPLinkModule::FSimpleOutputDevice::Clear()
{
    Log.Empty();
}
#pragma endregion InternalTypes


#pragma region Startup / Shutdown
void FHTTPLinkModule::StartupModule()
{
    GlobalLock = FPlatformProcess::NewInterprocessSynchObject(TEXT("Global\\ue-ist-httplink"), true);
    if (GlobalLock) {
        const uint64 MaxNanosecondsToWait = 10 * 1000000ULL; // 10ms
        if (!GlobalLock->TryLock(MaxNanosecondsToWait)) {
            // 他のプロセスが lock してる
            FPlatformProcess::DeleteInterprocessSynchObject(GlobalLock);
            GlobalLock = nullptr;
        }
    }

    if (GlobalLock) {
        GConfig->SetString(TEXT("HTTPServer.Listeners"), TEXT("DefaultBindAddress"), TEXT("any"), GEngineIni);

        // HTTP の listening 開始
        auto& HttpServerModule = FHttpServerModule::Get();
        Router = HttpServerModule.GetHttpRouter(PORT);

        TMap<FString, FHttpRequestHandler> Handlers;
#define AddHandler(Path, Func) Handlers.Add(Path, [this](auto& Request, auto& OnComplete) { return Func(Request, OnComplete); })

        AddHandler("/editor/exec", OnEditorExec);
        AddHandler("/editor/screenshot", OnEditorScreenshot);

        AddHandler("/actor/list", OnActorList);
        AddHandler("/actor/select", OnActorSelect);
        AddHandler("/actor/focus", OnActorFocus);
        AddHandler("/actor/create", OnActorCreate);
        AddHandler("/actor/delete", OnActorDelete);
        AddHandler("/actor/merge", OnActorMerge);

        AddHandler("/level/new", OnLevelNew);
        AddHandler("/level/load", OnLevelLoad);
        AddHandler("/level/save", OnLevelSave);

        AddHandler("/asset/list", OnAssetList);
        AddHandler("/asset/import", OnAssetImport);

#undef AddHandler

        for (auto& KVP : Handlers) {
            HRoutes.Push(
                Router->BindRoute(KVP.Key, EHttpServerRequestVerbs::VERB_GET | EHttpServerRequestVerbs::VERB_POST, KVP.Value)
            );
        }
        HttpServerModule.StartAllListeners();
    }


    // コンテキストメニュー登録
    auto& Extenders = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor")).GetAllLevelViewportContextMenuExtenders();
    Extenders.Add(FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateRaw(this, &FHTTPLinkModule::BuildActorContextMenu));
}

void FHTTPLinkModule::ShutdownModule()
{
    if (HScreenshot.IsValid()) {
        FScreenshotRequest::OnScreenshotRequestProcessed().Remove(HScreenshot);
        HScreenshot = {};
    }

    // コンテキストメニュー登録解除のうまい方法がわからず…
    //auto& Extenders = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor")).GetAllLevelViewportContextMenuExtenders();

    for (auto& H : HRoutes) {
        Router->UnbindRoute(H);
    }
    // 他への影響を考えて FHttpServerModule::Get().StopAllListeners() はしない

    if (GlobalLock) {
        GlobalLock->Unlock();
        FPlatformProcess::DeleteInterprocessSynchObject(GlobalLock);
        GlobalLock = nullptr;
    }
}

bool FHTTPLinkModule::Tick(float DeltaTime)
{
    return true;
}
#pragma endregion Startup / Shutdown


#pragma region ContextMenu
TSharedRef<FExtender> FHTTPLinkModule::BuildActorContextMenu(const TSharedRef<FUICommandList> CommandList, const TArray<AActor*> Actors)
{
    TSharedPtr<FExtender> Extender = MakeShareable(new FExtender());
    auto Build = [=](FMenuBuilder& Builder) {
        Builder.AddMenuEntry(
            LOCTEXT("CopyLinkAddress", "Copy Link Address"),
            {},
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "Actors.Attach"),
            FExecuteAction::CreateLambda([=]() { CopyLinkAddress(Actors); })
        );
    };
    Extender->AddMenuExtension("ActorTypeTools", EExtensionHook::After, CommandList,
        FMenuExtensionDelegate::CreateLambda(Build));
    return Extender.ToSharedRef();
}

void FHTTPLinkModule::CopyLinkAddress(const TArray<AActor*> Actors)
{
    if (!Actors.IsEmpty()) {
        auto Str = FString::Printf(TEXT("http://localhost:%d/actor/focus?guid=%s"), PORT, *Actors[0]->GetActorGuid().ToString());
        FPlatformApplicationMisc::ClipboardCopy(*Str);
        //UE_LOG(LogTemp, Log, TEXT("FHTTPLinkModule::CopyLinkAddress(): %s"), *Str);
    }
}
#pragma endregion ContextMenu


#pragma region Editor Commands
bool FHTTPLinkModule::OnEditorExec(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
{
    FString Command;
    if (GetQueryParam(Request, "c", Command) || GetQueryParam(Request, "command", Command)) {
        Outputs.Clear();
        GUnrealEd->Exec(GetEditorWorld(), *Command, Outputs);
    }
    return Respond(Result, Outputs.Log);
}

bool FHTTPLinkModule::OnEditorScreenshot(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
{
    static const FString ScreenshotDir = FPaths::ProjectIntermediateDir();
    static const FString ScreenshotPath = ScreenshotDir + TEXT("screenshot.png");

    auto& FS = IPlatformFile::GetPlatformPhysical();
    auto Timestamp = FS.GetTimeStampLocal(*ScreenshotPath);
    if (Timestamp == FDateTime::MinValue()) {
        if (!FS.CreateDirectoryTree(*ScreenshotDir)) {
            return Respond(Result);
        }
    }

    double TimeGap = (FDateTime::Now() - Timestamp).GetTotalSeconds();
    if (TimeGap < 5.0) {
        // 5 秒以内に撮影されたスクリーンショットならそれを返す
        return ServeFile(Result, ScreenshotPath, "image/png");
    }
    else {
        if (!bScreenshotInProgress) {
            if (!HScreenshot.IsValid()) {
                HScreenshot = FScreenshotRequest::OnScreenshotRequestProcessed().AddRaw(this, &FHTTPLinkModule::OnScreenshotProcessed);
            }

            bool ShowUI = true;
            GetQueryParam(Request, "ui", ShowUI);
            // 撮影リクエスト発行
            FScreenshotRequest::RequestScreenshot(ScreenshotPath, ShowUI, false);
            bScreenshotInProgress = true;

            // エディタがアクティブなウィンドウでないと撮影処理が進まないっぽいので…
            MakeEditorWindowForeground();
            GEditor->RedrawLevelEditingViewports();
        }

        // 撮影が完了していないので Retry-After を返してリトライしてもらう
        return RespondRetry(Result, "/editor/screenshot", 2);
    }
}

void FHTTPLinkModule::OnScreenshotProcessed()
{
    bScreenshotInProgress = false;
}
#pragma endregion Editor Commands


#pragma region Actor Commands
bool FHTTPLinkModule::OnActorList(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
{
    TArray<TSharedPtr<FJsonValue>> Json;
    EachActor(GetEditorWorld(), [&](AActor* Actor) {
        Json.Add(MakeShared<FJsonValueObject>(FActorSummary(Actor).ToJson()));
        });
    return RespondJson(Result, Json);
}

static TFunction<AActor* ()> GetActorFinder(const FHttpServerRequest& Request)
{
    auto* World = GetEditorWorld();
    if (!World) {
        return {};
    }

    FGuid GUID;
    FName Name;
    FString Label;
    if (GetQueryParam(Request, "guid", GUID)) {
        // GUID で 検索 (一意)
        return [World, GUID]() { return FindActor(World, [&](AActor* A) { return A->GetActorGuid() == GUID; }); };
    }
    else if (GetQueryParam(Request, "name", Name)) {
        // FName で検索 (一意)
        return [World, Name]() { return FindActor(World, [&](AActor* A) { return A->GetFName() == Name; }); };
    }
    else if (GetQueryParam(Request, "label", Label)) {
        // Label で検索 (一意ではない)
        return [World, Label]() { return FindActor(World, [&](AActor* A) { return A->GetActorLabel() == Label; }); };
    }
    // Unique ID は変動しうるので対応しない
    return {};
}

bool FHTTPLinkModule::OnActorSelect(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
{
    bool R = false;
    bool Additive = false;
    TFunction<AActor* ()> Finder = GetActorFinder(Request);
    GetQueryParam(Request, "additive", Additive);

    if (Finder) {
        if (!Additive) {
            GEditor->SelectNone(true, false);
        }
        if (AActor* Actor = Finder()) {
            GEditor->SelectActor(Actor, true, true, true);
            R = true;
        }
    }
    return Respond(Result, FString::Printf(TEXT("%d"), (int)R));
}

bool FHTTPLinkModule::OnActorFocus(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
{
    bool R = false;
    TFunction<AActor* ()> Finder = GetActorFinder(Request);

    if (Finder) {
        // アニメーションを見せるため Unreal Editor を最前面化
        MakeEditorWindowForeground();

        GEditor->SelectNone(true, false);
        if (AActor* Actor = Finder()) {
            GEditor->SelectActor(Actor, true, true, true);

            // Actor をフォーカス (F キーと同等の操作)
            GUnrealEd->Exec(GetEditorWorld(), TEXT("CAMERA ALIGN ACTIVEVIEWPORTONLY"));
            R = true;
        }
    }
    return Respond(Result, FString::Printf(TEXT("%d"), (int)R));
}

bool FHTTPLinkModule::OnActorCreate(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
{
    if (!GEditor) {
        return Respond(Result);
    }

    FString Label;
    FVector Location = FVector::Zero();
    FString AssetPath;
    GetQueryParam(Request, "label", Label);
    GetQueryParam(Request, "location", Location);
    GetQueryParam(Request, "assetpath", AssetPath);

    FString Ret;
    if (!AssetPath.IsEmpty()) {
        auto& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(AssetPath);
        if (AssetData.IsValid()) {
            auto UndoScope = FScopedTransaction(LOCTEXT("OnCreateActor", "OnCreateActor"));
            auto* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
            AActor* Actor = EditorActorSubsystem->SpawnActorFromObject(AssetData.GetAsset(), Location);
            if (Actor) {
                if (!Label.IsEmpty()) {
                    Actor->SetActorLabel(Label);
                }
                Ret = Actor->GetActorGuid().ToString();
            }
        }
    }
    return Respond(Result, Ret);
}

bool FHTTPLinkModule::OnActorDelete(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
{
    bool R = false;
    TFunction<AActor* ()> Finder = GetActorFinder(Request);

    if (Finder) {
        if (AActor* Actor = Finder()) {
            auto UndoScope = FScopedTransaction(LOCTEXT("OnDeleteActor", "OnDeleteActor"));
            Actor->Modify();
            Actor->Destroy();
            R = true;
        }
    }
    return Respond(Result, FString::Printf(TEXT("%d"), (int)R));
}

bool FHTTPLinkModule::OnActorMerge(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
{
    // todo
    return Respond(Result);
}
#pragma endregion Actor Commands


#pragma region Level Commands
bool FHTTPLinkModule::OnLevelNew(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
{
    if (!GEditor) {
        return Respond(Result);
    }

    bool R = false;
    FString AssetPath, TemplatePath;
    GetQueryParam(Request, "assetpath", AssetPath);
    GetQueryParam(Request, "templatepath", TemplatePath);
    if (!AssetPath.IsEmpty()) {
        auto LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();
        if (!TemplatePath.IsEmpty()) {
            R = LevelEditorSubsystem->NewLevelFromTemplate(AssetPath, TemplatePath);
        }
        else {
            R = LevelEditorSubsystem->NewLevel(AssetPath);
        }
    }
    return Respond(Result, FString::Printf(TEXT("%d"), (int)R));
}

bool FHTTPLinkModule::OnLevelLoad(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
{
    if (!GEditor) {
        return Respond(Result);
    }

    bool R = false;
    FString AssetPath;
    GetQueryParam(Request, "assetpath", AssetPath);

    if (!AssetPath.IsEmpty()) {
        auto LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();
        R = LevelEditorSubsystem->LoadLevel(AssetPath);
    }
    return Respond(Result, FString::Printf(TEXT("%d"), (int)R));
}

bool FHTTPLinkModule::OnLevelSave(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
{
    if (!GEditor) {
        return Respond(Result);
    }

    bool R = false;
    bool All = false;
    GetQueryParam(Request, "all", All);

    auto LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();
    if (All) {
        R = LevelEditorSubsystem->SaveAllDirtyLevels();
    }
    else {
        R = LevelEditorSubsystem->SaveCurrentLevel();
    }
    return Respond(Result, FString::Printf(TEXT("%d"), (int)R));
}
#pragma endregion Level Commands


#pragma region Asset Commands
bool FHTTPLinkModule::OnAssetList(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
{
    TArray<TSharedPtr<FJsonValue>> Json;
    auto& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    AssetRegistryModule.Get().EnumerateAllAssets([&](const FAssetData& Data) {
        Json.Add(MakeShared<FJsonValueObject>(FAssetSummary(Data).ToJson()));
        return true;
        });
    return RespondJson(Result, Json);
}

bool FHTTPLinkModule::OnAssetImport(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
{
    // todo
    return Respond(Result);
}
#pragma endregion Asset Commands


#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FHTTPLinkModule, HTTPLink)