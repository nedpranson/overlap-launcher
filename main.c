#include <windows.h>

__declspec(dllexport) void __overlap_ignore_proc(void) {}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
    case WM_DESTROY:
      PostQuitMessage(0);
      break;
  }

  return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    return 1;
  }

  HMODULE libModule = LoadLibraryA(argv[1]);
  if (!libModule) {
    return 1;
  }

  FARPROC proc = GetProcAddress(libModule, "__overlap_hook_proc");
  if (!proc) {
    FreeLibrary(libModule);
    return 1;
  }

  HINSTANCE hInstance = GetModuleHandleW(NULL);
  if (!hInstance) {
    FreeLibrary(libModule);
    return 1;
  }

  WNDCLASSW wndClass = {0};
  wndClass.lpfnWndProc = WndProc;
  wndClass.hInstance = hInstance;
  wndClass.lpszClassName = L"OverlapTrayClass";

  if (!RegisterClassW(&wndClass)) {
    FreeLibrary(libModule);
    return 1;
  };

  HWND hWnd = CreateWindowW(
      wndClass.lpszClassName,
      L"Overlap Tray Window",
      WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      640,
      480,
      NULL,
      NULL,
      hInstance,
      NULL
  );

  if (!hWnd) {
    FreeLibrary(libModule);
    return 1;
  }

  HICON icon = LoadImageW(
      NULL,
      (LPWSTR)IDI_APPLICATION,
      IMAGE_ICON,
      16,
      16,
      LR_SHARED
  );

  if (!icon) {
    FreeLibrary(libModule);
    return 1;
  }

  NOTIFYICONDATAW data = {0};
  data.cbSize = sizeof(data);
  data.hWnd = hWnd;
  data.uID = 1;
  data.uFlags = NIF_ICON | NIF_TIP;
  data.hIcon = icon;
  lstrcpyW(data.szTip, L"Overlap");

  if (!Shell_NotifyIconW(NIM_ADD, &data)) {
    FreeLibrary(libModule);
    return 1;
  }

  HHOOK hook = SetWindowsHookEx(WH_CBT, (HOOKPROC)proc, libModule, 0);
  if (!hook) {
    Shell_NotifyIconW(NIM_DELETE, &data);
    FreeLibrary(libModule);
    return 1;
  }

  MSG msg;
  while (GetMessageW(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  Shell_NotifyIconW(NIM_DELETE, &data);
  UnhookWindowsHookEx(hook);
  FreeLibrary(libModule);

  return 0;
}
