//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THE SOFTWARE IS PROVIDED �AS IS? WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
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

#ifdef _DEBUG
static unsigned char temp[4096 * 4096 * 4];
#endif

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
    m_framePool = Direct3D11CaptureFramePool::Create(
        m_device,
        DirectXPixelFormat::B8G8R8A8UIntNormalized,
        2,
		size);
    m_session = m_framePool.CreateCaptureSession(m_item);
    //m_session.IsCursorCaptureEnabled(false);
    m_lastSize = size;
	m_frameArrived = m_framePool.FrameArrived(auto_revoke, { this, &SimpleCapture::OnFrameArrived });
}

// Start sending capture frames
void SimpleCapture::StartCapture(unsigned char* framePtr)
{
    m_frameData = framePtr;
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
            m_swapChain->ResizeBuffers(
                2, 
				static_cast<uint32_t>(m_lastSize.Width),
				static_cast<uint32_t>(m_lastSize.Height),
                static_cast<DXGI_FORMAT>(DirectXPixelFormat::B8G8R8A8UIntNormalized), 
                0);
        }

        {
            auto frameSurface = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());
            /* need GetDesc because ContentSize is not reliable */
            D3D11_TEXTURE2D_DESC desc;
            frameSurface->GetDesc(&desc);
            UINT uiWidth = desc.Width;
            UINT uiHeight = desc.Height;

            com_ptr<ID3D11Texture2D> backBuffer;
            check_hresult(m_swapChain->GetBuffer(0, guid_of<ID3D11Texture2D>(), backBuffer.put_void()));
            m_d3dContext->CopyResource(backBuffer.get(), frameSurface.get());
            
            // Staging buffer/texture
            D3D11_TEXTURE2D_DESC CopyBufferDesc;
            CopyBufferDesc.Width = desc.Width;
            CopyBufferDesc.Height = desc.Height;
            CopyBufferDesc.MipLevels = 1;
            CopyBufferDesc.ArraySize = 1;
            CopyBufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            CopyBufferDesc.SampleDesc.Count = 1;
            CopyBufferDesc.SampleDesc.Quality = 0;
            CopyBufferDesc.Usage = D3D11_USAGE_STAGING;
            CopyBufferDesc.BindFlags = 0;
            CopyBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            CopyBufferDesc.MiscFlags = 0;

            ID3D11Texture2D* CopyBuffer = nullptr;
            auto d3dDevice = GetDXGIInterfaceFromObject<ID3D11Device>(m_device);
            hr = d3dDevice->CreateTexture2D(&CopyBufferDesc, nullptr, &CopyBuffer);
            if (FAILED(hr)) {
                OutputDebugStringA("Failed: d3dDevice->CreateTexture2D(&CopyBufferDesc, nullptr, &CopyBuffer);\r\n");
            }
            // Copy needed part of image
            m_d3dContext->CopySubresourceRegion(CopyBuffer, 0, 0, 0, 0, backBuffer.get(), 0, nullptr);
            // QI for IDXGISurface
            IDXGISurface* CopySurface = nullptr;
            hr = CopyBuffer->QueryInterface(__uuidof(IDXGISurface), (void**)&CopySurface);
            CopyBuffer->Release();
            CopyBuffer = nullptr;
            if (FAILED(hr))
            {
                return OutputDebugStringA("Failed: CopyBuffer->QueryInterface(__uuidof(IDXGISurface), (void**)&CopySurface);\r\n");
            }

            // Map pixels
            DXGI_MAPPED_RECT MappedSurface;
            hr = CopySurface->Map(&MappedSurface, DXGI_MAP_READ);
            if (FAILED(hr)) {
                OutputDebugStringA("Failed: CopySurface->Map(&MappedSurface, DXGI_MAP_READ);\r\n");
            }
            unsigned long ulFrameBufferSize = uiWidth * uiHeight * 4;
#ifdef _DEBUG
            memcpy(temp, MappedSurface.pBits, ulFrameBufferSize);
            memcpy(m_frameData, MappedSurface.pBits, ulFrameBufferSize);
#else
            memcpy(m_frameData, MappedSurface.pBits, ulFrameBufferSize);
#endif
            // Done with resource
            CopySurface->Unmap();
            CopySurface->Release();
            CopySurface = nullptr;
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
            2,
            m_lastSize);
    }
}

