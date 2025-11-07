#include "windef.h"
#include <minwindef.h>
#include <windows.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <dwmapi.h>

HWND get_owner_window(HWND window) {
  HWND owner = window;
  HWND tmp = NULL;

  while ((tmp = GetWindow(owner, GW_OWNER))) {
    owner = tmp;
  }

  return owner;
}

bool is_win10_background(HWND window) {
  // When DwmGetWindowAttribute we just want to return false by default.
  BOOL flag = FALSE;
  DwmGetWindowAttribute(window, DWMWA_CLOAKED, &flag, sizeof(BOOL));
  return flag;
}

static inline bool is_app_window(DWORD ex_styles) {
  return !((ex_styles & WS_EX_TOOLWINDOW) && !(ex_styles & WS_EX_APPWINDOW));
}

bool is_taskbar_window(HWND window) {
  if (IsWindowVisible(window)) {
    HWND owner = get_owner_window(window);
    assert(owner != NULL);
    
    if (GetLastActivePopup(owner) != window) {
      return false;
    }

    DWORD owner_styles = GetWindowLong(owner, GWL_EXSTYLE);
    if (owner_styles && IsWindowVisible(owner) && is_app_window(owner_styles) && !is_win10_background(owner)) {
      return true;
    }

    DWORD window_styles = GetWindowLong(window, GWL_EXSTYLE);
    if (window_styles && is_app_window(window_styles) && !is_win10_background(window)) {
      return true;
    }

    if (owner_styles == 0 && window_styles == 0) {
      return true;
    }
  }
  return false;
}

BOOL CALLBACK loop_windows(HWND hWnd, LPARAM lParam) {
  (void)lParam;

  if (is_taskbar_window(hWnd)) {
    int len = GetWindowTextLength(hWnd);
    if (len == 0) goto blk;

    char* title = (char*)malloc(len + 1);
    if (title == NULL) goto blk;

    GetWindowText(hWnd, title, len + 1);

    printf("%s\n", title);
    free(title);
  }

blk:

  return TRUE;
}

VOID Wineventproc(
  HWINEVENTHOOK hWinEventHook,
  DWORD event,
  HWND hwnd,
  LONG idObject,
  LONG idChild,
  DWORD idEventThread,
  DWORD dwmsEventTime
) {
  (void)hWinEventHook;
  (void)event;
  (void)idObject;
  (void)idChild;
  (void)idEventThread;
  (void)dwmsEventTime;

  if (is_taskbar_window(hwnd)) {
    int len = GetWindowTextLength(hwnd);
    if (len == 0) return;

    char* title = (char*)malloc(len + 1);
    if (title == NULL) return;

    GetWindowText(hwnd, title, len + 1);

    printf("Focusing on: %s\n", title);
    free(title);
  }
}

int main(void) {
  EnumWindows(loop_windows, 0);

  HWINEVENTHOOK hook = SetWinEventHook(
      EVENT_OBJECT_FOCUS,
      EVENT_OBJECT_FOCUS,
      NULL,
      Wineventproc,
      0,
      0,
      WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
  );

  if (!hook) {
    return 1;
  }

  return 0;
}
