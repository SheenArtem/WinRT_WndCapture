//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THE SOFTWARE IS PROVIDED ?AS IS? WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH 
// THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//*********************************************************

#include "pch.h"
#include "App.h"

using namespace winrt;
using namespace Windows::System;
using namespace Windows::Foundation;
using namespace Windows::UI;
using namespace Windows::UI::Composition;
using namespace Windows::Graphics::Capture;

void App::Initialize(
    ContainerVisual const& root)
{
    auto queue = DispatcherQueue::GetForCurrentThread();

    m_compositor = root.Compositor();
    m_root = m_compositor.CreateContainerVisual();
    m_content = m_compositor.CreateSpriteVisual();
    m_brush = m_compositor.CreateSurfaceBrush();

    m_root.RelativeSizeAdjustment({ 1, 1 });
    root.Children().InsertAtTop(m_root);

    m_content.AnchorPoint({ 0.5f, 0.5f });
    m_content.RelativeOffsetAdjustment({ 0.5f, 0.5f, 0 });
    m_content.RelativeSizeAdjustment({ 1, 1 });
    m_content.Size({ -80, -80 });
    m_content.Brush(m_brush);
    m_brush.HorizontalAlignmentRatio(0.5f);
    m_brush.VerticalAlignmentRatio(0.5f);
    m_brush.Stretch(CompositionStretch::Uniform);
    auto shadow = m_compositor.CreateDropShadow();
    shadow.Mask(m_brush);
    m_content.Shadow(shadow);
    m_root.Children().InsertAtTop(m_content);

    auto d3dDevice = CreateD3DDevice();
    auto dxgiDevice = d3dDevice.as<IDXGIDevice>();
    m_device = CreateDirect3DDevice(dxgiDevice.get());
    if (m_device == NULL)
        OutputDebugStringA("CreateDirect3DDevice(dxgiDevice.get()); return NULL!!! \r\n");
}

void App::StartCapture(HWND hwnd)
{
	if (m_capture)
	{
		m_capture->Close();
		m_capture = nullptr;
	}
    try {
        auto windowHide = false;
        bool windowMinimized;
        WINDOWPLACEMENT placemant = { 0 };
        GetWindowPlacement(hwnd, &placemant);
        switch (placemant.showCmd)
        {
        case SW_SHOW:
        case SW_SHOWDEFAULT:
        case SW_SHOWMAXIMIZED:
        case SW_SHOWNOACTIVATE:
        case SW_SHOWNA:
        case SW_SHOWNORMAL:
            windowMinimized = false;
            break;
        default:
            windowMinimized = true;
        }
        if (windowMinimized) return;
        auto item = CreateCaptureItemForWindow(hwnd);

        m_capture = std::make_unique<SimpleCapture>(m_device, item);

        auto surface = m_capture->CreateSurface(m_compositor);
        m_brush.Surface(surface);

        m_capture->StartCapture();
    }
    catch (...) {
        OutputDebugStringA("Capture target window failed!!!\r\n");
    }
}

winrt::Windows::Graphics::SizeInt32 App::GetFrameSize()
{
    winrt::Windows::Graphics::SizeInt32 size; 
    size.Height = 0; 
    size.Width = 0; 
    return m_capture != nullptr ? m_capture->GetLastSize() : size;
}

bool App::CopyImage(unsigned char* buf)
{    
    return m_capture == nullptr ? false : m_capture->CopyImage(buf);
}