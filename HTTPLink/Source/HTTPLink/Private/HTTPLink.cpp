﻿#include "HTTPLink.h"
#include "./JsonUtils.h"

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
#include "Serialization/MemoryWriter.h"
#include "EditorClassUtils.h"


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
// int32
template<>
inline bool GetQueryParam(const FHttpServerRequest& Request, const char* Name, int& Dst)
{
    if (auto* V = Request.QueryParams.Find(Name)) {
        if (V->StartsWith("0x"))
            Dst = FParse::HexNumber(**V);
        else
            Dst = FCString::Atoi(**V);
        return true;
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
// FQuat
template<>
inline bool GetQueryParam(const FHttpServerRequest& Request, const char* Name, FQuat& Dst)
{
    if (auto* V = Request.QueryParams.Find(Name)) {
        float X, Y, Z, W;
        if (swscanf_s(**V, TEXT("%f,%f,%f,%f"), &X, &Y, &Z, &W) == 4) {
            Dst = FQuat(X, Y, Z, W);
        }
        return true;
    }
    return false;
}


struct ParamHandler
{
    const char* Name;
    TFunction<bool(const FHttpServerRequest& Request)> FromRequest;
    TFunction<bool(const JObject& Json)> FromJson;

    template<class T>
    ParamHandler(const char* InName, T& Dst)
        : Name(InName)
    {
        FromRequest = [this, &Dst](const FHttpServerRequest& Request) {
            return GetQueryParam(Request, Name, Dst);
        };
        FromJson = [this, &Dst](const JObject& Json) {
            return Json.Get(Name, Dst);
        };
    }
};

template<class... T>
static TArray<FString> GetQueryParamsImpl(const FHttpServerRequest& Request, T&&... PlaceholdersList)
{
    TArray<FString> Ret;
    FString JsonStr;
    if (GetQueryParam(Request, "json", JsonStr)) {
        JObject JsonObj = JObject::Parse(JsonStr);
        auto HandleJson = [&](auto& Placeholders) {
            for (auto& P : Placeholders) {
                if (P.FromJson(JsonObj)) {
                    Ret.Add(P.Name);
                }
            }
        };
        ([&] { HandleJson(PlaceholdersList); } (), ...);
    }
    else {
        auto HandleQueryParams = [&](auto& Placeholders) {
            for (auto& P : Placeholders) {
                if (P.FromRequest(Request)) {
                    Ret.Add(P.Name);
                }
            }
        };
        ([&] { HandleQueryParams(PlaceholdersList); } (), ...);
    }
    return Ret;
}
template<class... T>
static TArray<FString> GetQueryParams(const FHttpServerRequest& Request, std::initializer_list<ParamHandler>&& Placeholders, T&&... Additional)
{
    return GetQueryParamsImpl(Request, Placeholders, Forward<T>(Additional)...);
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

static bool Serve(const FHttpResultCallback& Result, const FString& Content = "", const FString& ContentType = "text/plain")
{
    // FHttpServerResponse::Ok() は 204 No Content を返すので使わない方がいい
    auto Response = FHttpServerResponse::Create(Content, ContentType);
    Response->Code = EHttpServerResponseCodes::Ok;
    AddAccessControl(*Response);
    Result(MoveTemp(Response));
    return true;
}

static bool Serve(const FHttpResultCallback& Result, TArray<uint8>&& Content, const FString& ContentType)
{
    auto Response = FHttpServerResponse::Create(MoveTemp(Content), ContentType);
    Response->Code = EHttpServerResponseCodes::Ok;
    AddAccessControl(*Response);
    Result(MoveTemp(Response));
    return true;
}

template<class T>
static bool ServeJsonImpl(const FHttpResultCallback& Result, T&& Json)
{
    TArray<uint8> Data;
    FMemoryWriter MemWriter(Data);
    FJsonSerializer::Serialize(Json, TJsonWriterFactory<UTF8CHAR>::Create(&MemWriter));
    return Serve(Result, MoveTemp(Data), "application/json");
}
static bool ServeJson(const FHttpResultCallback& Result, JObject&& Json)
{
    return ServeJsonImpl(Result, MoveTemp(Json));
}
static bool ServeJson(const FHttpResultCallback& Result, JArray&& Json)
{
    return ServeJsonImpl(Result, MoveTemp(Json));
}
static bool ServeJson(const FHttpResultCallback& Result, std::initializer_list<JObject::Field>&& Fields)
{
    return ServeJsonImpl(Result, JObject(MoveTemp(Fields)));
}
static bool ServeJson(const FHttpResultCallback& Result, bool R)
{
    return ServeJson(Result, { {"result", R} });
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

static bool ServeRetry(const FHttpResultCallback& Result, FString Location, int Second)
{
    auto Response = FHttpServerResponse::Create("", "text/plain");
    Response->Code = EHttpServerResponseCodes::Redirect;
    Response->Headers.Add("Location", { Location });
    Response->Headers.Add("Retry-After", { FString::Printf(TEXT("%d"), Second) });
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

static IAssetRegistry& GetAssetRegistry()
{
    static IAssetRegistry& Instance = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
    return Instance;
}

static FAssetData GetAssetByObjectPath(FString Path)
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
    return GetAssetRegistry().GetAssetByObjectPath(FSoftObjectPath(Path));
#else
    return GetAssetRegistry().GetAssetByObjectPath(FName(Path));
#endif
}

static TArray<FAssetData> GetAssetsByClass(const FName& ClassName)
{
    TArray<FAssetData> Ret;
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
    GetAssetRegistry().GetAssetsByClass(UClass::TryConvertShortTypeNameToPathName<UStruct>(*ClassName.ToString()), Ret);
#else
    GetAssetRegistry().GetAssetsByClass(ClassName, Ret);
#endif
    return Ret;
}
template<class T>
static TArray<FAssetData> GetAssetsByClass()
{
    return GetAssetsByClass(T::StaticClass()->GetFName());
}

static UEditorActorSubsystem* GetEditorActorSubsystem()
{
    static UEditorActorSubsystem* Instance = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    return Instance;
}
#pragma endregion Utilities


#pragma region InternalTypes
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
        AddHandler("/actor/transform", OnActorTransform);

        AddHandler("/level/new", OnLevelNew);
        AddHandler("/level/load", OnLevelLoad);
        AddHandler("/level/save", OnLevelSave);

        AddHandler("/asset/list", OnAssetList);
        AddHandler("/asset/import", OnAssetImport);

        AddHandler("/test", OnTest);

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
    GetQueryParams(Request, { {"c", Command}, {"command", Command} });
    if (!Command.IsEmpty()) {
        Outputs.Clear();
        GUnrealEd->Exec(GetEditorWorld(), *Command, Outputs);
    }
    return ServeJson(Result, { {"outputs", Outputs.Log} });
}

bool FHTTPLinkModule::OnEditorScreenshot(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
{
    static const FString ScreenshotDir = FPaths::ProjectIntermediateDir();
    static const FString ScreenshotPath = ScreenshotDir + TEXT("screenshot.png");

    auto& FS = IPlatformFile::GetPlatformPhysical();
    auto Timestamp = FS.GetTimeStampLocal(*ScreenshotPath);
    if (Timestamp == FDateTime::MinValue()) {
        if (!FS.CreateDirectoryTree(*ScreenshotDir)) {
            return Serve(Result);
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
            GetQueryParams(Request, { {"ui", ShowUI} });
            // 撮影リクエスト発行
            FScreenshotRequest::RequestScreenshot(ScreenshotPath, ShowUI, false);
            bScreenshotInProgress = true;

            // エディタがアクティブなウィンドウでないと撮影処理が進まないっぽいので…
            MakeEditorWindowForeground();
            GEditor->RedrawLevelEditingViewports();
        }

        // 撮影が完了していないので Retry-After を返してリトライしてもらう
        return ServeRetry(Result, "/editor/screenshot", 2);
    }
}

void FHTTPLinkModule::OnScreenshotProcessed()
{
    bScreenshotInProgress = false;
}
#pragma endregion Editor Commands


#pragma region Actor Commands
static JObject MakeActorSummary(AActor* Actor)
{
    if (!Actor) {
        return {};
    }

    JObject Ret({
        { "typeName", Actor->GetClass()->GetName() },
        { "label", Actor->GetActorLabel() },
        { "name", Actor->GetFName() },
        { "guid", Actor->GetActorGuid() },
        { "transform", Actor->GetActorTransform() },
        });

    JArray Components;
    for (auto& C : Actor->GetComponents()) {
        Components.Add(JObject({
            { "typeName", C->GetClass()->GetName() },
            { "name", C->GetName() },
            }));
    }
    Ret["components"] = Components;
    return Ret;
}

bool FHTTPLinkModule::OnActorList(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
{
    JArray Json;
    EachActor(GetEditorWorld(), [&](AActor* Actor) {
        Json.Add(MakeActorSummary(Actor));
        });
    return ServeJson(Result, MoveTemp(Json));
}

static TFunction<AActor* ()> GetActorFinder(const FHttpServerRequest& Request, std::initializer_list<ParamHandler>&& Additional = {})
{
    auto* World = GetEditorWorld();
    if (!World) {
        return {};
    }

    FGuid GUID;
    FName Name;
    FString Label;
    GetQueryParams(Request, {
        { "guid", GUID },  {"name", Name}, {"label", Label}
        }, MoveTemp(Additional));

    if (GUID.IsValid()) {
        // GUID で 検索 (一意)
        return [World, GUID]() { return FindActor(World, [&](AActor* A) { return A->GetActorGuid() == GUID; }); };
    }
    else if (!Name.IsNone()) {
        // FName で検索 (一意)
        return [World, Name]() { return FindActor(World, [&](AActor* A) { return A->GetFName() == Name; }); };
    }
    else if (!Label.IsEmpty()) {
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
    TFunction<AActor* ()> Finder = GetActorFinder(Request, { {"additive", Additive} });

    if (Finder) {
        if (!Additive) {
            GEditor->SelectNone(true, false);
        }
        if (AActor* Actor = Finder()) {
            GEditor->SelectActor(Actor, true, true, true);
            R = true;
        }
    }
    return ServeJson(Result, R);
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
    return ServeJson(Result, R);
}

bool FHTTPLinkModule::OnActorCreate(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
{
    if (!GEditor) {
        return Serve(Result);
    }

    FString Label;
    FVector Location = FVector::Zero();
    FString AssetPath;
    FString ClassName; // 接辞語を除いた class 名 (例: "Actor", "StaticMeshActor", etc)
    GetQueryParams(Request, {
        { "label", Label }, { "location", Location }, { "assetPath", AssetPath}, { "className", ClassName}
        });

    AActor* Actor = nullptr;
    auto SpawnActorScope = [&](auto&& SpawnActor) {
        auto UndoScope = FScopedTransaction(LOCTEXT("OnCreateActor", "OnCreateActor"));
        Actor = SpawnActor();
        if (Actor && !Label.IsEmpty()) {
            Actor->SetActorLabel(Label);
        }
    };

    if (!AssetPath.IsEmpty()) {
        FAssetData AssetData = GetAssetByObjectPath(AssetPath);
        if (AssetData.IsValid()) {
            SpawnActorScope([&]() {
                return GetEditorActorSubsystem()->SpawnActorFromObject(AssetData.GetAsset(), Location);
                });
        }
    }
    else if (!ClassName.IsEmpty()) {
        if (UClass* C = FEditorClassUtils::GetClassFromString(ClassName)) {
            SpawnActorScope([&]() {
                return GetEditorActorSubsystem()->SpawnActorFromClass(C, Location);
                });
        }
    }

    JObject Json;
    Json["result"] = Actor ? true : false;
    if (Actor) {
        Json["actor"] = MakeActorSummary(Actor);
    }
    return ServeJson(Result, MoveTemp(Json));
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
    return ServeJson(Result, R);
}

bool FHTTPLinkModule::OnActorMerge(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
{
    // todo
    return ServeJson(Result, false);
}

bool FHTTPLinkModule::OnActorTransform(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
{
    bool R = false;
    AActor* Target = nullptr;
    if (auto Finder = GetActorFinder(Request)) {
        Target = Finder();
    }
    if (Target) {
        FVector Translation;
        FQuat Rotation;
        FVector Scale;
        bool Absolute = true;
        auto Set = GetQueryParams(Request, {
            { "t", Translation }, { "r", Rotation }, { "s", Scale}, { "abs", Absolute},
            });

        if (Set.Contains("s")) {
            if (!Absolute) {
                Scale = Target->GetActorScale() * Scale;
            }
            Target->SetActorScale3D(Scale);
            R = true;
        }
        if (Set.Contains("r")) {
            if (!Absolute) {
                Rotation = Target->GetActorRotation().Quaternion() * Rotation;
            }
            Target->SetActorRotation(Rotation);
            R = true;
        }
        if (Set.Contains("t")) {
            if (!Absolute) {
                Translation = Target->GetActorLocation() + Translation;
            }
            Target->SetActorLocation(Translation);
            R = true;
        }
    }
    return ServeJson(Result, R);
}
#pragma endregion Actor Commands


#pragma region Level Commands
bool FHTTPLinkModule::OnLevelNew(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
{
    if (!GEditor) {
        return ServeJson(Result, false);
    }

    bool R = false;
    FString AssetPath, TemplatePath;
    GetQueryParams(Request, {
        { "assetpath", AssetPath }, { "templatepath", TemplatePath}
        });

    if (!AssetPath.IsEmpty()) {
        auto LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();
        if (!TemplatePath.IsEmpty()) {
            R = LevelEditorSubsystem->NewLevelFromTemplate(AssetPath, TemplatePath);
        }
        else {
            R = LevelEditorSubsystem->NewLevel(AssetPath);
        }
    }
    return ServeJson(Result, R);
}

bool FHTTPLinkModule::OnLevelLoad(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
{
    if (!GEditor) {
        return ServeJson(Result, false);
    }

    bool R = false;
    FString AssetPath;
    GetQueryParams(Request, {
        { "assetpath", AssetPath },
        });

    if (!AssetPath.IsEmpty()) {
        auto LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();
        R = LevelEditorSubsystem->LoadLevel(AssetPath);
    }
    return ServeJson(Result, R);
}

bool FHTTPLinkModule::OnLevelSave(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
{
    if (!GEditor) {
        return ServeJson(Result, false);
    }

    bool R = false;
    bool All = false;
    GetQueryParams(Request, {
        { "all", All },
        });

    auto LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();
    if (All) {
        R = LevelEditorSubsystem->SaveAllDirtyLevels();
    }
    else {
        R = LevelEditorSubsystem->SaveCurrentLevel();
    }
    return ServeJson(Result, R);
}
#pragma endregion Level Commands


#pragma region Asset Commands
static JObject MakeAssetSummary(const FAssetData& Data)
{
    return JObject({
        { "typeName", Data.GetClass()->GetName() },
        { "assetName", Data.AssetName },
        { "packageName", Data.PackageName },
        { "objectPath", GetObectPathStr(Data) },
        });
}

bool FHTTPLinkModule::OnAssetList(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
{
    JArray Json;
    auto& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    AssetRegistryModule.Get().EnumerateAllAssets([&](const FAssetData& Data) {
        Json.Add(MakeAssetSummary(Data));
        return true;
        });
    return ServeJson(Result, MoveTemp(Json));
}

bool FHTTPLinkModule::OnAssetImport(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
{
    // todo
    return ServeJson(Result, false);
}
#pragma endregion Asset Commands


#pragma region Test Commands
bool FHTTPLinkModule::OnTest(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
{
#if (UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT || UE_BUILD_TEST)
    FString Case;
    GetQueryParams(Request, {
        { "case", Case },
        });

    if (Case == "json") {
        JObject Json;
        {
            TMap<FString, FString> Tmp;
            Tmp.Add("Key", "Value");
            Tmp.Add(TEXT("日本語Key"), TEXT("日本語Value"));
            Json["stringStringMap"] = Tmp;
        }
        {
            TMap<FGuid, TArray<int>> Tmp;
            Tmp.Add(FGuid::NewGuid(), { 0,1,2 });
            Tmp.Add(FGuid::NewGuid(), { 3,4,5 });
            Json["guidIntArrayMap"] = Tmp;
        }
        {
            TMap<FName, int> Tmp;
            Tmp.Add("tmpMapField1", 1);
            Tmp.Add("tmpMapField2", 2);
            Json += Tmp;
        }
        {
            TMap<int, int> Tmp;
            Tmp.Add(100, 1);
            Json += Tmp;
        }
        Json += {
            {"ansicharField", "ANSICHAR*"},
            { "tcharField", TEXT("TCHAR*") },
            { "guidField", FGuid::NewGuid() },
            { "dateTimeField", FDateTime::Now() },
            { "boolArrayField", {true, false, true} },
            { "enumField", EUnit::Bytes },
            { "vectorArrayField", {FVector(0,1,2), FVector(3,4,5)} },
            { "multipleTypeArrayField", MakeTuple(true, 100, "str", FVector(100,200,300)) },
            { FDateTime::Now(), true },
            { FGuid::NewGuid(), true },
        };
        Json[FName("proxyWithFNameKey")] = { 0,1,2,3 };
        Json["proxyWithTupleValues"] = MakeTuple(true, 100, "str", FVector(100, 200, 300));

        JArray Jarray;
        Jarray.Add(true, 1, "str");
        Jarray += {2, 3};
        Jarray += MakeTuple(FVector(0, 1, 2), FGuid::NewGuid(), EUnit::Bytes, std::string("std::string"));
        Json[std::string("testJArray")] = Jarray.ToValue();

        for (auto& KVP : Json) {
            UE_LOG(LogTemp, Log, TEXT("%s"), *KVP.Key);
        }
        {
            TMap<FGuid, TArray<int>> GuidIntArrayMap;
            FVector VectorArray[2];
            TTuple<bool, int, FString, FVector> MultiTypeArray;
            bool _1; int _2; FString _3; int _4; int _5; FVector _6; FGuid _7; EUnit _8; std::string _9;

            Json["guidIntArrayMap"] >> GuidIntArrayMap;
            Json["vectorArrayField"] >> VectorArray;
            Json["multipleTypeArrayField"] >> MultiTypeArray;
            Json["testJArray"] >> Tie(_1, _2, _3, _4, _5, _6, _7, _8, _9);
            UE_LOG(LogTemp, Log, TEXT("%s"), ANSI_TO_TCHAR(_9.c_str()));
        }
        return ServeJson(Result, MoveTemp(Json));
    }
    else {
    }
#endif

    return ServeJson(Result, false);
}
#pragma endregion Test Commands


#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FHTTPLinkModule, HTTPLink)