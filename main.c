#include <windows.h>

int main(int argc, char* argv[]) {
  if (argc != 2) {
    return 1;
  }

  HMODULE lib = LoadLibrary(argv[1]);
  if (!lib) {
    return 1;
  }

  FARPROC proc = GetProcAddress(lib, "__overlap_hook_proc");
  if (!proc) {
    FreeLibrary(lib);
    return 1;
  }

  HHOOK hook = SetWindowsHookEx(WH_CBT, (HOOKPROC)proc, lib, 0);
  if (!hook) {
    FreeLibrary(lib);
    return 1;
  }

  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {}

  UnhookWindowsHookEx(hook);
  FreeLibrary(lib);

  return 0;
}
