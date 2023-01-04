#include "pch.h"
#include "Win32WindowEnumeration.h"
#include "App.h"
#include "WindowCaptureAPI.h"

using namespace winrt;
using namespace Windows::UI;
using namespace Windows::UI::Composition;
using namespace Windows::UI::Composition::Desktop;

#define BUF_SIZE				4096*4096*4	//!!!!!!!!!!!!!!!! Aware of original resolution bigger than 4K
#define CAP_PIX_FMT_RGB32		1

typedef struct
{
    INT		TargetWindowIndex;
    HWND	WindowHandle;
    UINT	Width;
    UINT	Height;
    std::shared_ptr<App> m_APP;
    std::vector<Window> m_allWindow;
    std::shared_ptr<unsigned char> m_frameData;
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
    init_apartment(apartment_type::single_threaded);
    wndcap->m_frameData = std::make_shared<unsigned char>(BUF_SIZE);

    // Create a DispatcherQueue for our thread
    auto controller = CreateDispatcherQueueController();

    // Initialize Composition
    auto compositor = Compositor();
    auto target = CreateDesktopWindowTarget(compositor, wndcap->WindowHandle);
    auto root = compositor.CreateContainerVisual();
    root.RelativeSizeAdjustment({ 1.0f, 1.0f });
    target.Root(root);

    // Enqueue our capture work on the dispatcher
    auto queue = controller.DispatcherQueue();
    auto success = queue.TryEnqueue([=]() -> void
    {
        wndcap->m_APP->Initialize(root);
    });
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
    memcpy(buf, (unsigned char*)wndcap->m_frameData.get(), uiWidth * uiHeight * 4);

    return true;
}
