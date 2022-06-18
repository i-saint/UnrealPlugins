#include "HTTPLink.h"

#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "EngineUtils.h"
#include "LevelEditor.h"
#include "HAL/PlatformApplicationMisc.h"
#include "GenericPlatform/GenericPlatformApplicationMisc.h"

#if PLATFORM_WINDOWS
#include "IDesktopPlatform.h"
#include "Developer/DesktopPlatform/Private/Windows/DesktopPlatformWindows.h"
#endif

#define LOCTEXT_NAMESPACE "FHTTPLinkModule"

void FHTTPLinkModule::StartupModule()
{
    // HTTP の listening 開始
    auto& HttpServerModule = FHttpServerModule::Get();
    m_Router = HttpServerModule.GetHttpRouter(PORT);
    m_HFocus = m_Router->BindRoute(FString("/focus"), EHttpServerRequestVerbs::VERB_GET | EHttpServerRequestVerbs::VERB_POST,
        [this](auto& Request, auto& Result) { return Focus(Request, Result); });
    HttpServerModule.StartAllListeners();


    // コンテキストメニュー登録
    auto& Extenders = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor")).GetAllLevelViewportContextMenuExtenders();
    Extenders.Add(FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateRaw(this, &FHTTPLinkModule::BuildContextMenu));
}

void FHTTPLinkModule::ShutdownModule()
{
    // コンテキストメニュー登録解除のうまい方法がわからず…
    //auto& Extenders = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor")).GetAllLevelViewportContextMenuExtenders();

    m_Router->UnbindRoute(m_HFocus);
    m_Router = {};
    m_HFocus = {};
    // 他への影響を考えて FHttpServerModule::Get().StopAllListeners() はしない
}


#pragma region Utilities

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
    auto Window = FSlateApplication::Get().FindBestParentWindowForDialogs(nullptr, ESlateParentWindowSearchMethod::MainWindow);
    if (Window) {
#if PLATFORM_WINDOWS
        HWND HW = (HWND)Window->GetNativeWindow()->GetOSWindowHandle();
        DWORD FromTID = ::GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
        DWORD ToTID = ::GetWindowThreadProcessId(HW, nullptr);
        ::AttachThreadInput(FromTID, ToTID, true);
        ::SetForegroundWindow(HW); // == Window->GetNativeWindow()->HACK_ForceToFront()
        if (FromTID != ToTID) {
            ::AttachThreadInput(FromTID, ToTID, false);
        }
#else
        Window->GetNativeWindow()->HACK_ForceToFront();
#endif
    }
}

#pragma endregion Utilities


bool FHTTPLinkModule::Respond(const FHttpResultCallback& Result, FString&& Content)
{
    // FHttpServerResponse::Ok() は 204 No Content を返すので使わない方がいい
    auto Response = FHttpServerResponse::Create(MoveTemp(Content), "text/plain");
    Response->Code = EHttpServerResponseCodes::Ok;
    Result(MoveTemp(Response));
    return true;
}

bool FHTTPLinkModule::Focus(const FHttpServerRequest& Request, const FHttpResultCallback& Result)
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

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FHTTPLinkModule, HTTPLink)