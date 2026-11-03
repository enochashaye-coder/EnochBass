// TinySoundFont (MIT licence, see ThirdParty/TinySoundFont-LICENSE.txt) - the implementation is compiled once, here.
#define TSF_IMPLEMENTATION
#include "ThirdParty/tsf.h"

// v3.5: the thread that closes a hardware MIDI port keeps this plugin's DLL loaded until it is done, so removing the plugin
// can never unload code that thread is still running (Windows only; elsewhere these do nothing).
#ifdef _WIN32
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
void* enochPinModule()
{
    HMODULE h = nullptr;
    if (GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCWSTR> (&enochPinModule), &h)) return (void*) h;
    return nullptr;
}
void enochUnpinModuleAndExitThread (void* module)
{
    if (module != nullptr) FreeLibraryAndExitThread ((HMODULE) module, 0);
}
#else
void* enochPinModule() { return nullptr; }
void enochUnpinModuleAndExitThread (void*) {}
#endif
