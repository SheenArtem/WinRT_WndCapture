#include "pch.h"
#include "Win32WindowEnumeration.h"
#include "App.h"
#include "WindowCaptureAPI.h"

using namespace winrt;
using namespace Windows::UI;
using namespace Windows::UI::Composition;
using namespace Windows::UI::Composition::Desktop;

#define BUF_SIZE				4096*4096*4	//!!!!!!!!!!!!!!!! Aware of original resolution bigger than 4K

static unsigned char rawdata[BUF_SIZE] = { 0 };
typedef struct
{
    INT		TargetWindowIndex;
    HWND	WindowHandle;
    UINT	Width;
    UINT	Height;
    bool    capture_cursor;//TODO
    bool    cursor_visible;//TODO
    unsigned char* m_frameData;
    std::shared_ptr<App> m_APP;
    std::vector<Window> m_allWindow;
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
    wndcap->TargetWindowIndex = 0;
    wndcap->WindowHandle = WindowHandle;
    wndcap->Width = 0;
    wndcap->Height = 0;

    wndcap->m_APP = std::make_shared<App>();
    wndcap->m_allWindow = EnumerateWindows();
    // Init COM
    init_apartment(apartment_type::multi_threaded);
    wndcap->m_frameData = rawdata;

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
    //winrt::uninit_apartment(); //Needed?
    return true;
}

void StartCapture(WNDCAP_HANDLE wndcap_handle, int index)
{
    WNDCAP_HANDLE_STRUCT* wndcap = reinterpret_cast<WNDCAP_HANDLE_STRUCT*>(wndcap_handle);
    if (wndcap == nullptr)
        return;
    wndcap->m_APP->StartCapture(wndcap->m_allWindow[index].Hwnd(), wndcap->m_frameData);
}

int GetWindowCount(WNDCAP_HANDLE wndcap_handle)
{
    WNDCAP_HANDLE_STRUCT* wndcap = reinterpret_cast<WNDCAP_HANDLE_STRUCT*>(wndcap_handle);
    if (wndcap == nullptr)
        return 0;
    wndcap->m_allWindow = EnumerateWindows();
    return  wndcap->m_allWindow.size();
}

const wchar_t* GetWindowTitle(WNDCAP_HANDLE wndcap_handle, int index)
{
    WNDCAP_HANDLE_STRUCT* wndcap = reinterpret_cast<WNDCAP_HANDLE_STRUCT*>(wndcap_handle);
    if (wndcap == nullptr)
        return nullptr;
    
    if(wndcap->m_allWindow.size() == 0 || index >= wndcap->m_allWindow.size())
        wndcap->m_allWindow = EnumerateWindows();
    if (index >= wndcap->m_allWindow.size() || index < 0) {
        OutputDebugStringA("Wrong index - GetWindowTitle()");
        return nullptr; 
    }
    else {
        return wndcap->m_allWindow[index].Title().c_str();
    }
}

bool WindowCapture(WNDCAP_HANDLE wndcap_handle, unsigned char* buf, unsigned int& uiWidth, unsigned int& uiHeight, bool bSkipMouse, bool& bMouseVisible)
{
    WNDCAP_HANDLE_STRUCT* wndcap = reinterpret_cast<WNDCAP_HANDLE_STRUCT*>(wndcap_handle);
    if (wndcap == nullptr)
        return false;

    winrt::Windows::Graphics::SizeInt32 frameSize = wndcap->m_APP->GetFrameSize();
    uiWidth = frameSize.Width;
    uiHeight = frameSize.Height;
    memcpy(buf, wndcap->m_frameData, uiWidth * uiHeight * 4);

    return true;
}


#ifdef _DEBUG
int CALLBACK WinMain(
HINSTANCE instance,
HINSTANCE previousInstance,
LPSTR     cmdLine,
int       cmdShow);

auto g_app = std::make_shared<App>();
auto g_windows = EnumerateWindows();
static unsigned char g_ptrData[BUF_SIZE];
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

    memset(g_ptrData, 0, BUF_SIZE);
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
            
            g_app->StartCapture(window.Hwnd(), g_ptrData);
        }
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
        break;
    }

    return 0;
}
#endif