//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THE SOFTWARE IS PROVIDED “AS IS? WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH 
// THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//*********************************************************

#include "pch.h"
#include "SimpleCapture.h"

using namespace winrt;
using namespace Windows;
using namespace Windows::Foundation;
using namespace Windows::System;
using namespace Windows::Graphics;
using namespace Windows::Graphics::Capture;
using namespace Windows::Graphics::DirectX;
using namespace Windows::Graphics::DirectX::Direct3D11;
using namespace Windows::Foundation::Numerics;
using namespace Windows::UI;
using namespace Windows::UI::Composition;

SimpleCapture::SimpleCapture(
    IDirect3DDevice const& device,
    GraphicsCaptureItem const& item)
{
    m_item = item;
    m_device = device;
	// Set up 
    auto d3dDevice = GetDXGIInterfaceFromObject<ID3D11Device>(m_device);
    d3dDevice->GetImmediateContext(m_d3dContext.put());

	auto size = m_item.Size();
    
    m_swapChain = CreateDXGISwapChain(
        d3dDevice, 
		static_cast<uint32_t>(size.Width),
		static_cast<uint32_t>(size.Height),
        static_cast<DXGI_FORMAT>(DirectXPixelFormat::B8G8R8A8UIntNormalized),
        2);

	// Create framepool, define pixel format (DXGI_FORMAT_B8G8R8A8_UNORM), and frame size.
#ifdef UI_THREAD_CAPTURE
    m_framePool = Direct3D11CaptureFramePool::Create(
        m_device,
        DirectXPixelFormat::B8G8R8A8UIntNormalized,
        1,
        size);
#else
    m_framePool = Direct3D11CaptureFramePool::CreateFreeThreaded(
        m_device,
        DirectXPixelFormat::B8G8R8A8UIntNormalized,
        1,
        size);
#endif
    m_session = m_framePool.CreateCaptureSession(m_item);
    //m_session.IsCursorCaptureEnabled(false);
    m_lastSize = size;
#ifdef UI_THREAD_CAPTURE
	m_frameArrived = m_framePool.FrameArrived(auto_revoke, { this, &SimpleCapture::OnFrameArrived });
#endif
}

// Start sending capture frames
void SimpleCapture::StartCapture()
{
    CheckClosed();
    m_session.StartCapture();
}

ICompositionSurface SimpleCapture::CreateSurface(
    Compositor const& compositor)
{
    CheckClosed();
    return CreateCompositionSurfaceForSwapChain(compositor, m_swapChain.get());
}

// Process captured frames
void SimpleCapture::Close()
{
    auto expected = false;
    if (m_closed.compare_exchange_strong(expected, true))
    {
		m_frameArrived.revoke();
		m_framePool.Close();
        m_session.Close();

        m_swapChain = nullptr;
        m_framePool = nullptr;
        m_session = nullptr;
        m_item = nullptr;
    }
}

void SimpleCapture::CopyImage(unsigned char* buf)
{
    auto newSize = false;
    auto d3dDevice = GetDXGIInterfaceFromObject<ID3D11Device>(m_device);
    auto frame = m_framePool.TryGetNextFrame();
    auto frameContentSize = frame.ContentSize();
    m_captureFrame = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());
    
    D3D11_TEXTURE2D_DESC desc;
    m_captureFrame->GetDesc(&desc);
    UINT uiWidth = desc.Width;
    UINT uiHeight = desc.Height;
    ULONG ulFrameBufferSize = uiWidth * uiHeight * 4;

    auto CopyBuffer = CreateStageTexture2D(d3dDevice,
        static_cast<uint32_t>(desc.Width),
        static_cast<uint32_t>(desc.Height),
        static_cast<DXGI_FORMAT>(DirectXPixelFormat::B8G8R8A8UIntNormalized));
    m_d3dContext->CopyResource(CopyBuffer.get(), m_captureFrame.get());

    //Copy the bits
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    winrt::check_hresult(m_d3dContext->Map(CopyBuffer.get(), 0, D3D11_MAP_READ, 0, &mapped));
    auto source = reinterpret_cast<byte*>(mapped.pData);
    for (auto i = 0; i < (int)desc.Height; i++)
    {
        memcpy(buf, source, desc.Width * 4);
        source += mapped.RowPitch;
        buf += desc.Width * 4;
    }
    m_d3dContext->Unmap(CopyBuffer.get(), 0);

    if (frameContentSize.Width != m_lastSize.Width ||
        frameContentSize.Height != m_lastSize.Height)
    {
        // The thing we have been capturing has changed size.
        // We need to resize our swap chain first, then blit the pixels.
        // After we do that, retire the frame and then recreate our frame pool.
        newSize = true;
        m_lastSize = frameContentSize;
        m_framePool.Recreate(
            m_device,
            DirectXPixelFormat::B8G8R8A8UIntNormalized,
            1,
            m_lastSize);
    }
}

void SimpleCapture::OnFrameArrived(
    Direct3D11CaptureFramePool const& sender,
    winrt::Windows::Foundation::IInspectable const&)
{
    auto newSize = false;
    HRESULT hr = S_OK;
    {
        auto frame = sender.TryGetNextFrame();
		auto frameContentSize = frame.ContentSize();

        if (frameContentSize.Width != m_lastSize.Width ||
			frameContentSize.Height != m_lastSize.Height)
        {
            // The thing we have been capturing has changed size.
            // We need to resize our swap chain first, then blit the pixels.
            // After we do that, retire the frame and then recreate our frame pool.
            newSize = true;
            m_lastSize = frameContentSize;
#ifdef _DEBUG
            m_swapChain->ResizeBuffers(
                2, 
				static_cast<uint32_t>(m_lastSize.Width),
				static_cast<uint32_t>(m_lastSize.Height),
                static_cast<DXGI_FORMAT>(DirectXPixelFormat::B8G8R8A8UIntNormalized), 
                0);
#endif
        }
        
        {
            m_captureFrame = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());
            auto d3dDevice = GetDXGIInterfaceFromObject<ID3D11Device>(m_device);
            /* need GetDesc because ContentSize is not reliable */
            D3D11_TEXTURE2D_DESC desc;
            m_captureFrame->GetDesc(&desc);
            UINT uiWidth = desc.Width;
            UINT uiHeight = desc.Height;
            ULONG ulFrameBufferSize = uiWidth * uiHeight * 4;
#ifdef _DEBUG
            com_ptr<ID3D11Texture2D> backBuffer;
            check_hresult(m_swapChain->GetBuffer(0, guid_of<ID3D11Texture2D>(), backBuffer.put_void()));
            m_d3dContext->CopyResource(backBuffer.get(), m_captureFrame.get());
#endif            
        }
    }
#ifdef _DEBUG
    DXGI_PRESENT_PARAMETERS presentParameters = { 0 };
    m_swapChain->Present1(1, 0, &presentParameters);
#endif
    if (newSize)
    {
        m_framePool.Recreate(
            m_device,
            DirectXPixelFormat::B8G8R8A8UIntNormalized,
            1,
            m_lastSize);
    }
}