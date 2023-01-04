#pragma once

#if defined(WINDOWCAPTURE_EXPORTS)
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT __declspec(dllimport)
#endif

typedef void* WNDCAP_HANDLE;
//TODO: mouse cursor
DLLEXPORT WNDCAP_HANDLE Init(HWND WindowHandle);
DLLEXPORT bool UninitWNDCAP(WNDCAP_HANDLE wndcap_handle);
DLLEXPORT void StartCapture(WNDCAP_HANDLE wndcap_handle, int index);
DLLEXPORT const std::vector<std::wstring> GetAllWindowTitle(WNDCAP_HANDLE wndcap_handle);
DLLEXPORT bool WindowCapture(WNDCAP_HANDLE wndcap_handle, unsigned char* buf, unsigned int& uiWidth, unsigned int& uiHeight, bool bSkipMouse, bool& bMouseVisible);