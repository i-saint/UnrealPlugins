#include "HTTPLink.h"

#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "EngineUtils.h"
#include "LevelEditor.h"
#include "LevelEditorSubsystem.h"
#include "HAL/PlatformApplicationMisc.h"
#include "GenericPlatform/GenericPlatformApplicationMisc.h"

#if PLATFORM_WINDOWS
#include "IDesktopPlatform.h"
#include "Developer/DesktopPlatform/Private/Windows/DesktopPlatformWindows.h"
#endif

#define LOCTEXT_NAMESPACE "FHTTPLinkModule"

#pragma region FSimpleOutputDevice
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
#pragma endregion FSimpleOutputDevice


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

        AddHandler("/focus", OnFocus);
        AddHandler("/exec", OnExec);

        AddHandler("/level/new", OnNewLevel);
        AddHandler("/level/load", OnLoadLevel);
        AddHandler("/level/save", OnSaveLevel);

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
    Extenders.Add(FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateRaw(this, &FHTTPLinkModule::BuildContextMenu));
}

void FHTTPLinkModule::ShutdownModule()
{
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
#pragma endregion Startup / Shutdown


#pragma region Utilities
static UWorld* GetEditorWorld()
{
    return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

template<class ActorType = AActor, class Cond>
inline ActorType* FindActor(UWorld* World, Cond&& cond)
{
    auto It = TActorIterator<ActorType>(World);
    while (It) {
        if (cond(*It))
            return *It;
        ++It;
    }
    return nullptr;
}

template<class T>
inline bool GetQueryParam(const FHttpServerRequest& Request, const char* Name, T& Dst);
// bool
template<>
inline bool GetQueryParam(const FHttpServerRequest& Request, const char* Name, bool& Dst)
{
    if (auto* v = Request.QueryParams.Find(Name)) {
        Dst = v->ToBool();
    }
    return false;
}
// int64
template<>
inline bool GetQueryParam(const FHttpServerRequest& Request, const char* Name, int64& Dst)
{
    if (auto* v = Request.QueryParams.Find(Name)) {
        if (v->StartsWith("0x"))
            Dst = FParse::HexNumber64(**v);
        else
            Dst = FCString::Atoi64(**v);
        return true;
    }
    return false;
}
// FString
template<>
inline bool GetQueryParam(const FHttpServerRequest& Request, const char* Name, FString& Dst)
{
    if (auto* v = Request.QueryParams.Find(Name)) {
        Dst = *v;
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
#pragma endregion Utilities


#pragma region ContextMenu
TSharedRef<FExtender> FHTTPLinkModule::BuildContextMenu(const TSharedRef<FUICommandList> CommandList, const TArray<AActor*> Actors)
{
    TSharedPtr<FExtender> Extender = MakeShareable(new FExtender());
    auto Build = [=](FMenuBuilder& Builder) {
        Builder.BeginSection("ist", LOCTEXT("ist", "ist"));
        Builder.AddMenuEntry(
            LOCTEXT("CopyLinkAddress", "リンクのアドレスをコピー"),
            {},
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "Actors.Attach"),
            FExecuteAction::CreateLambda([=]() { CopyLinkAddress(Actors); })
        );
        Builder.EndSection();
    };
    Extender->AddMenuExtension("ActorTypeTools", EExtensionHook::After, CommandList,
        FMenuExtensionDelegate::CreateLambda(Build));
    return Extender.ToSharedRef();
}

void FHTTPLinkModule::CopyLinkAddress(const TArray<AActor*> Actors)
{
    if (!Actors.IsEmpty()) {
        auto Str = FString::Printf(TEXT("http://localhost:%d/focus?id=0x%08x"), PORT, Actors[0]->GetUniqueID());
        FPlatformApplicationMisc::ClipboardCopy(*Str);
        UE_LOG(LogTemp, Log, TEXT("FHTTPLinkModule::CopyLinkAddress(): %s"), *Str);
    }
}
#pragma endregion ContextMenu


#pragma region HTTP Command
bool FHTTPLinkModule::Respond(const FHttpResultCallback& Result, const FString& Content)
{
    // FHttpServerResponse::Ok() は 204 No Content を返すので使わない方がいい
    auto Response = FHttpServerResponse::Create(Content, "text/plain");
    Response->Code = EHttpServerResponseCodes::Ok;
    Result(MoveTemp(Response));
    return true;
}

bool FHTTPLinkModule::OnFocus(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
{
    auto* World = GEditor->GetEditorWorldContext().World();
    if (!World) {
        return Respond(Result);
    }

    AActor* Target = nullptr;
    {
        FString Name;
        int64 ID = 0;
        if (GetQueryParam(Request, "name", Name)) {
            // 名前で actor を検索
            Target = FindActor(World, [&](AActor* a) { return a->GetName() == Name; });
        }
        if (GetQueryParam(Request, "id", ID)) {
            // unique ID で actor を検索
            Target = FindActor(World, [&](AActor* a) { return a->GetUniqueID() == ID; });
        }
    }
    if (Target) {
        // アニメーションを見せるため Unreal Editor を最前面化
        MakeEditorWindowForeground();

        // Target 選択
        GEditor->SelectNone(true, false);
        GEditor->SelectActor(Target, true, true, true);

        // Target をフォーカス (F キーと同等の操作)
        GUnrealEd->Exec(World, TEXT("CAMERA ALIGN ACTIVEVIEWPORTONLY"));
    }

    return Respond(Result);
}

bool FHTTPLinkModule::OnExec(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
{
    FString Command;
    if (GetQueryParam(Request, "c", Command) || GetQueryParam(Request, "command", Command)) {
        Outputs.Clear();
        GUnrealEd->Exec(GetEditorWorld(), *Command, Outputs);
    }
    return Respond(Result, Outputs.Log);
}

bool FHTTPLinkModule::OnNewLevel(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
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

bool FHTTPLinkModule::OnLoadLevel(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
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

bool FHTTPLinkModule::OnSaveLevel(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
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
#pragma endregion HTTP Command

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FHTTPLinkModule, HTTPLink)