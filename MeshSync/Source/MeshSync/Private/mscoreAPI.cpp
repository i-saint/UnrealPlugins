#include "mscoreAPI.h"

#include "GenericPlatform/GenericPlatformProcess.h"

#define msAPI(Func, Ret, ...) Ret (*Func)(__VA_ARGS__)
#include "mscoreAPI.inl"
#undef msAPI

bool LoadMeshSyncLibrary()
{
    auto mod = FPlatformProcess::GetDllHandle(TEXT("mscore.dll"));
    if (!mod)
        return false;

#define msAPI(Func, ...) (void*&)Func = FPlatformProcess::GetDllExport(mod, TEXT(#Func)); if(!Func) return false;
#include "mscoreAPI.inl"
#undef msAPI

    return true;
}
