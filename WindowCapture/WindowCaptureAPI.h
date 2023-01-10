#pragma once

#if defined(WINDOWCAPTURE_EXPORTS)
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void* WNDCAP_HANDLE;
//TODO: mouse cursor
DLLEXPORT WNDCAP_HANDLE InitWndCap(HWND WindowHandle);
DLLEXPORT bool UninitWndCap(WNDCAP_HANDLE wndcap_handle);
DLLEXPORT void StartCapture(WNDCAP_HANDLE wndcap_handle, HWND wndHandle);
DLLEXPORT bool WindowCapture(WNDCAP_HANDLE wndcap_handle, unsigned char* buf, unsigned int& uiWidth, unsigned int& uiHeight, bool bSkipMouse, bool& bMouseVisible);

#ifdef __cplusplus
}
#endif
