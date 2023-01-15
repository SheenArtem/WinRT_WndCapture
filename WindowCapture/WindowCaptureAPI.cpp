#include "pch.h"
#include "Win32WindowEnumeration.h"
#include "App.h"
#include "WindowCaptureAPI.h"

using namespace winrt;
using namespace Windows::UI;
using namespace Windows::UI::Composition;
using namespace Windows::UI::Composition::Desktop;

typedef struct
{
    HWND	WindowHandle;
    UINT	Width;
    UINT	Height;
    bool    capture_cursor;//TODO
    bool    cursor_visible;//TODO
    std::shared_ptr<App> m_APP;
} WNDCAP_HANDLE_STRUCT;

// Direct3D11CaptureFramePool requires a DispatcherQueue
auto CreateDispatcherQueueController()
{
    namespace abi = ABI::Windows::System;

    DispatcherQueueOptions options
    {
        sizeof(DispatcherQueueOptions),
        DQTYPE_THREAD_CURRENT,
        DQTAT_COM_STA
    };

    Windows::System::DispatcherQueueController controller{ nullptr };
    check_hresult(CreateDispatcherQueueController(options, reinterpret_cast<abi::IDispatcherQueueController**>(put_abi(controller))));
    return controller;
}

DesktopWindowTarget CreateDesktopWindowTarget(Compositor const& compositor, HWND window)
{
    namespace abi = ABI::Windows::UI::Composition::Desktop;

    auto interop = compositor.as<abi::ICompositorDesktopInterop>();
    DesktopWindowTarget target{ nullptr };
    check_hresult(interop->CreateDesktopWindowTarget(window, true, reinterpret_cast<abi::IDesktopWindowTarget**>(put_abi(target))));
    return target;
}

WNDCAP_HANDLE InitWndCap(HWND WindowHandle)
{
    WNDCAP_HANDLE_STRUCT* wndcap = new WNDCAP_HANDLE_STRUCT;
    if (wndcap == nullptr)
        return nullptr;
    wndcap->WindowHandle = WindowHandle;
    wndcap->Width = 0;
    wndcap->Height = 0;

    wndcap->m_APP = std::make_shared<App>();
    // Init COM
    //init_apartment(apartment_type::multi_threaded);

    // Create a DispatcherQueue for our thread
    auto controller = CreateDispatcherQueueController();

    // Initialize Composition
    auto compositor = Compositor();
    auto target = CreateDesktopWindowTarget(compositor, wndcap->WindowHandle);
    auto root = compositor.CreateContainerVisual();
    root.RelativeSizeAdjustment({ 1.0f, 1.0f });
    target.Root(root);
    wndcap->m_APP->Initialize(root);
    return wndcap;
}

bool UninitWndCap(WNDCAP_HANDLE wndcap_handle)
{
    WNDCAP_HANDLE_STRUCT* wndcap = reinterpret_cast<WNDCAP_HANDLE_STRUCT*>(wndcap_handle);
    if (wndcap == nullptr)
        return false;
    delete wndcap;
    return true;
}

void StartCapture(WNDCAP_HANDLE wndcap_handle, HWND wndHandle)
{
    WNDCAP_HANDLE_STRUCT* wndcap = reinterpret_cast<WNDCAP_HANDLE_STRUCT*>(wndcap_handle);
    if (wndcap == nullptr)
        return;
    wndcap->m_APP->StartCapture(wndHandle);
}

bool WindowCapture(WNDCAP_HANDLE wndcap_handle, unsigned char* buf, unsigned int& uiWidth, unsigned int& uiHeight, bool bSkipMouse, bool& bMouseVisible)
{
    WNDCAP_HANDLE_STRUCT* wndcap = reinterpret_cast<WNDCAP_HANDLE_STRUCT*>(wndcap_handle);
    if (wndcap == nullptr)
        return false;

    bool ret = wndcap->m_APP->CopyImage(buf);
    winrt::Windows::Graphics::SizeInt32 frameSize = wndcap->m_APP->GetFrameSize();
    uiWidth = frameSize.Width;
    uiHeight = frameSize.Height;
    return ret;
}


#ifdef _DEBUG
int CALLBACK WinMain(
HINSTANCE instance,
HINSTANCE previousInstance,
LPSTR     cmdLine,
int       cmdShow);

auto g_app = std::make_shared<App>();
auto g_windows = EnumerateWindows();
LRESULT CALLBACK WndProc(
    HWND   hwnd,
    UINT   msg,
    WPARAM wParam,
    LPARAM lParam);

int CALLBACK WinMain(
    HINSTANCE instance,
    HINSTANCE previousInstance,
    LPSTR     cmdLine,
    int       cmdShow)
{
    // Init COM
    init_apartment(apartment_type::single_threaded);

    // Create the window
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = instance;
    wcex.hIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_APPLICATION));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = L"ScreenCaptureforHWND";
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_APPLICATION));
    WINRT_VERIFY(RegisterClassEx(&wcex));

    HWND hwnd = CreateWindow(
        L"ScreenCaptureforHWND",
        L"ScreenCaptureforHWND",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        800,
        600,
        NULL,
        NULL,
        instance,
        NULL);
    WINRT_VERIFY(hwnd);

    ShowWindow(hwnd, cmdShow);
    UpdateWindow(hwnd);

    // Create combo box
    HWND comboBoxHwnd = CreateWindow(
        WC_COMBOBOX,
        L"",
        CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE,
        10,
        10,
        200,
        200,
        hwnd,
        NULL,
        instance,
        NULL);
    WINRT_VERIFY(comboBoxHwnd);

    // Populate combo box
    for (auto& window : g_windows)
    {
        SendMessage(comboBoxHwnd, CB_ADDSTRING, 0, (LPARAM)window.Title().c_str());
    }
    //SendMessage(comboBoxHwnd, CB_SETCURSEL, 0, 0);

    // Create a DispatcherQueue for our thread
    auto controller = CreateDispatcherQueueController();

    // Initialize Composition
    auto compositor = Compositor();
    auto target = CreateDesktopWindowTarget(compositor, hwnd);
    auto root = compositor.CreateContainerVisual();
    root.RelativeSizeAdjustment({ 1.0f, 1.0f });
    target.Root(root);

    // Enqueue our capture work on the dispatcher
    auto queue = controller.DispatcherQueue();
    auto success = queue.TryEnqueue([=]() -> void
        {
            g_app->Initialize(root);
        });
    WINRT_VERIFY(success);

    // Message pump
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(
    HWND   hwnd,
    UINT   msg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_COMMAND:
        if (HIWORD(wParam) == CBN_SELCHANGE)
        {
            auto index = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
            auto window = g_windows[index];
            
            g_app->StartCapture(window.Hwnd());
        }
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
        break;
    }

    return 0;
}
#endif