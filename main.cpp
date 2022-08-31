#include "pch.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define DEBUG_BUILD

static bool global_windowDidResize = false;
static UINT g_deskWidth = 0;
static UINT g_deskHeight = 0;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;
    switch(msg)
    {
        case WM_KEYDOWN:
        {
            if(wparam == VK_ESCAPE)
                DestroyWindow(hwnd);
            break;
        }
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            break;
        }
        case WM_SIZE:
        {
            global_windowDidResize = true;
            break;
        }
        default:
            result = DefWindowProcW(hwnd, msg, wparam, lparam);
    }
    return result;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int /*nShowCmd*/)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // #00_OpenWindow
    HWND hwnd;
    {
		WNDCLASSEXW winClass = {};
		winClass.cbSize = sizeof(WNDCLASSEXW);
		winClass.style = CS_HREDRAW | CS_VREDRAW;
		winClass.lpfnWndProc = &WndProc;
		winClass.hInstance = hInstance;
		winClass.hIcon = LoadIconW(0, IDI_APPLICATION);
		winClass.hCursor = LoadCursorW(0, IDC_ARROW);
		winClass.lpszClassName = L"MyWindowClass";
		winClass.hIconSm = LoadIconW(0, IDI_APPLICATION);

		if (!RegisterClassExW(&winClass)) {
			MessageBoxA(0, "RegisterClassEx failed", "Fatal Error", MB_OK);
			return GetLastError();
		}

		RECT initialRect = { 0, 0, 800, 600 };
		AdjustWindowRectEx(&initialRect, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_OVERLAPPEDWINDOW);
		LONG initialWidth = initialRect.right - initialRect.left;
		LONG initialHeight = initialRect.bottom - initialRect.top;

		hwnd = CreateWindowExW(WS_EX_OVERLAPPEDWINDOW,
			winClass.lpszClassName,
			L"Desktop Duplication",
			WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			CW_USEDEFAULT, CW_USEDEFAULT,
			initialWidth,
			initialHeight,
			0, 0, hInstance, 0);

		if (!hwnd) {
			MessageBoxA(0, "CreateWindowEx failed", "Fatal Error", MB_OK);
			return GetLastError();
		}
    }

    // #01_CreateDevice
    // Create D3D11 Device and Context
    ID3D11Device1* d3d11Device = nullptr;
    ID3D11DeviceContext1* d3d11DeviceContext = nullptr;
    {
        ID3D11Device* baseDevice;
        ID3D11DeviceContext* baseDeviceContext;
        D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
        UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        #if defined(DEBUG_BUILD)
        creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
        #endif

        HRESULT hResult = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 
                                            0, creationFlags, 
                                            featureLevels, ARRAYSIZE(featureLevels), 
                                            D3D11_SDK_VERSION, &baseDevice, 
                                            0, &baseDeviceContext);
        if(FAILED(hResult)){
            MessageBoxA(0, "D3D11CreateDevice() failed", "Fatal Error", MB_OK);
            return GetLastError();
        }
        
        // Get 1.1 interface of D3D11 Device and Context
        hResult = baseDevice->QueryInterface(__uuidof(ID3D11Device1), (void**)&d3d11Device);
        assert(SUCCEEDED(hResult));
        baseDevice->Release();

        hResult = baseDeviceContext->QueryInterface(__uuidof(ID3D11DeviceContext1), (void**)&d3d11DeviceContext);
        assert(SUCCEEDED(hResult));
        baseDeviceContext->Release();
    }

#ifdef DEBUG_BUILD
    // Set up debug layer to break on D3D11 errors
    ID3D11Debug *d3dDebug = nullptr;
    d3d11Device->QueryInterface(__uuidof(ID3D11Debug), (void**)&d3dDebug);
    if (d3dDebug)
    {
        ID3D11InfoQueue *d3dInfoQueue = nullptr;
        if (SUCCEEDED(d3dDebug->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&d3dInfoQueue)))
        {
            d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
            d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
            d3dInfoQueue->Release();
        }
        d3dDebug->Release();
    }
#endif
    IDXGIOutputDuplication* outputDup = nullptr;

    // Create Swap Chain
    // #02_CreateSwapChain
    IDXGISwapChain1* d3d11SwapChain;
    {
        // Get DXGI Factory (needed to create Swap Chain)
        IDXGIFactory2* dxgiFactory;
        {
            IDXGIDevice1* dxgiDevice;
            HRESULT hResult = d3d11Device->QueryInterface(__uuidof(IDXGIDevice1), (void**)&dxgiDevice);
            assert(SUCCEEDED(hResult));

            IDXGIAdapter* dxgiAdapter;
            hResult = dxgiDevice->GetAdapter(&dxgiAdapter);
            assert(SUCCEEDED(hResult));
            dxgiDevice->Release();

            // #DoubleCursor-DXGI
			IDXGIOutput* output = nullptr;
            IDXGIOutput5* output5Array[5];
			IDXGIOutput5* output5 = nullptr;

            int numOutput = 0;
			while (dxgiAdapter->EnumOutputs(numOutput, &output) != DXGI_ERROR_NOT_FOUND)
			{
				DXGI_OUTPUT_DESC desc;
				output->GetDesc(&desc);

				std::wstring text = L"***Output: ";
				text += desc.DeviceName;
				text += L"\n";
				OutputDebugString(text.c_str());

				HRESULT h2 = output->QueryInterface(IID_PPV_ARGS(&output5));
                assert(SUCCEEDED(h2));

                output5Array[numOutput] = output5;

				output->Release();
                numOutput++;
			}
			
            output5 = output5Array[0];
            //output5 = output5Array[1];

            const DXGI_FORMAT formats[]{
                DXGI_FORMAT_B8G8R8A8_UNORM,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                DXGI_FORMAT_R10G10B10A2_UNORM,
            };
            //#21_CreateDuplicateOutput
            bool oldWay = false;
            if (oldWay)
            {
                output5->DuplicateOutput(d3d11Device, &outputDup);
            }
            else
            {
                HRESULT hr5 = output5->DuplicateOutput1(d3d11Device,
                    0,
                    ARRAYSIZE(formats),
                    formats,
                    &outputDup);

                if (FAILED(hr5))
                {
                    HRESULT hError = hr5;
                    assert(false);
                }
            }
			output5->Release();

			DXGI_OUTDUPL_DESC odd;
			outputDup->GetDesc(&odd);

			std::ostringstream sout;
			sout << "Width :" << odd.ModeDesc.Width << "\n";
			sout << "Height:" << odd.ModeDesc.Height << "\n";
			OutputDebugStringA(sout.str().c_str());

            DXGI_ADAPTER_DESC adapterDesc;
            dxgiAdapter->GetDesc(&adapterDesc);

            OutputDebugStringA("Graphics Device: ");
            OutputDebugStringW(adapterDesc.Description);

            hResult = dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), (void**)&dxgiFactory);
            assert(SUCCEEDED(hResult));
            dxgiAdapter->Release();
        }

        DXGI_SWAP_CHAIN_DESC1 d3d11SwapChainDesc = {};
        d3d11SwapChainDesc.Width = 0; // use window width
        d3d11SwapChainDesc.Height = 0; // use window height
        d3d11SwapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        d3d11SwapChainDesc.SampleDesc.Count = 1;
        d3d11SwapChainDesc.SampleDesc.Quality = 0;
        d3d11SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        d3d11SwapChainDesc.BufferCount = 2;
        d3d11SwapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        d3d11SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        d3d11SwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        d3d11SwapChainDesc.Flags = 0;

        HRESULT hResult = dxgiFactory->CreateSwapChainForHwnd(d3d11Device, hwnd, &d3d11SwapChainDesc, 0, 0, &d3d11SwapChain);
        assert(SUCCEEDED(hResult));

        dxgiFactory->Release();
    }

    // Create Framebuffer Render Target
    // #03_FrameBufferRTV
    ID3D11RenderTargetView* d3d11FrameBufferView;
    {
        ID3D11Texture2D* d3d11FrameBuffer;
        HRESULT hResult = d3d11SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&d3d11FrameBuffer);
        assert(SUCCEEDED(hResult));

        hResult = d3d11Device->CreateRenderTargetView(d3d11FrameBuffer, 0, &d3d11FrameBufferView);
        assert(SUCCEEDED(hResult));
        d3d11FrameBuffer->Release();
    }

    // Create Vertex Shader
    // #05_VertexShader
    ID3DBlob* vsBlob;
    ID3D11VertexShader* vertexShader;
    {
        ID3DBlob* shaderCompileErrorsBlob;
        HRESULT hResult = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "vs_main", "vs_5_0", 0, 0, &vsBlob, &shaderCompileErrorsBlob);
        if(FAILED(hResult))
        {
            const char* errorString = NULL;
            if(hResult == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
                errorString = "Could not compile shader; file not found";
            else if(shaderCompileErrorsBlob){
                errorString = (const char*)shaderCompileErrorsBlob->GetBufferPointer();
                shaderCompileErrorsBlob->Release();
            }
            MessageBoxA(0, errorString, "Shader Compiler Error", MB_ICONERROR | MB_OK);
            return 1;
        }

        hResult = d3d11Device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);
        assert(SUCCEEDED(hResult));
    }

    // Create Pixel Shader
    // #06_PixelShader
    ID3D11PixelShader* pixelShader;
    {
        ID3DBlob* psBlob;
        ID3DBlob* shaderCompileErrorsBlob;
        HRESULT hResult = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "ps_main", "ps_5_0", 0, 0, &psBlob, &shaderCompileErrorsBlob);
        if(FAILED(hResult))
        {
            const char* errorString = NULL;
            if(hResult == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
                errorString = "Could not compile shader; file not found";
            else if(shaderCompileErrorsBlob){
                errorString = (const char*)shaderCompileErrorsBlob->GetBufferPointer();
                shaderCompileErrorsBlob->Release();
            }
            MessageBoxA(0, errorString, "Shader Compiler Error", MB_ICONERROR | MB_OK);
            return 1;
        }

        hResult = d3d11Device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader);
        assert(SUCCEEDED(hResult));
        psBlob->Release();
    }

    // Create Input Layout
    // #08_InputLayout
    ID3D11InputLayout* inputLayout;
    {
        D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
        {
            { "POS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEX", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };

        HRESULT hResult = d3d11Device->CreateInputLayout(inputElementDesc, ARRAYSIZE(inputElementDesc), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);
        assert(SUCCEEDED(hResult));
        vsBlob->Release();
    }

    // Create Vertex Buffer
    // #09_VertexBuffer
    ID3D11Buffer* vertexBuffer;
	const float vertexData[] = { // x, y, u, v
	    -1.0f,  1.0f, 0.f, 0.f,
	     1.0f, -1.0f, 1.f, 1.f,
	    -1.0f, -1.0f, 0.f, 1.f,
	    -1.0f,  1.0f, 0.f, 0.f,
	     1.0f,  1.0f, 1.f, 0.f,
	     1.0f, -1.0f, 1.f, 1.f
	};
	const UINT stride = 4 * sizeof(float);
	const UINT numVerts = sizeof(vertexData) / stride;
	const UINT offset = 0;
    {
        D3D11_BUFFER_DESC vertexBufferDesc {};
        vertexBufferDesc.ByteWidth = sizeof(vertexData);
        vertexBufferDesc.Usage     = D3D11_USAGE_IMMUTABLE;
        vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vertexSubresourceData = { vertexData };

        HRESULT hResult = d3d11Device->CreateBuffer(&vertexBufferDesc, &vertexSubresourceData, &vertexBuffer);
        assert(SUCCEEDED(hResult));
    }

    // Create Sampler State
    // #10_SamplerState
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter         = D3D11_FILTER_MIN_MAG_MIP_POINT;
    samplerDesc.AddressU       = D3D11_TEXTURE_ADDRESS_BORDER;
    samplerDesc.AddressV       = D3D11_TEXTURE_ADDRESS_BORDER;
    samplerDesc.AddressW       = D3D11_TEXTURE_ADDRESS_BORDER;
    samplerDesc.BorderColor[0] = 1.0f;
    samplerDesc.BorderColor[1] = 1.0f;
    samplerDesc.BorderColor[2] = 1.0f;
    samplerDesc.BorderColor[3] = 1.0f;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

    ID3D11SamplerState* samplerState;
    d3d11Device->CreateSamplerState(&samplerDesc, &samplerState);

    // Load Image
    //Luke's monitor Width = 3440, Height = 1440
    UINT texWidth = 1280;
    UINT texHeight = 720;
    UINT texNumChannels = 0;
    int texForceNumChannels = 4;
    //unsigned char* testTextureBytes = stbi_load("testTexture.png", &texWidth, &texHeight,
    //                                            &texNumChannels, texForceNumChannels);
    //assert(testTextureBytes);
    int texBytesPerRow = 4 * texWidth;

    // Create Texture
    // #11_CreateTexture
    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width              = texWidth;
    textureDesc.Height             = texHeight;
    textureDesc.MipLevels          = 1;
    textureDesc.ArraySize          = 1;
    textureDesc.Format             = DXGI_FORMAT_B8G8R8A8_UNORM; //DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count   = 1;
    textureDesc.Usage              = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags          = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

    D3D11_SUBRESOURCE_DATA textureSubresourceData = {};
    //textureSubresourceData.pSysMem = testTextureBytes;
    //textureSubresourceData.SysMemPitch = texBytesPerRow;

    ID3D11Texture2D* textureSDR = nullptr;
    ID3D11ShaderResourceView* textureSdrSRV = nullptr;
    ID3D11RenderTargetView* textureSdrRTV = nullptr;

    //d3d11Device->CreateTexture2D(&textureDesc, &textureSubresourceData, &texture);
    d3d11Device->CreateTexture2D(&textureDesc, nullptr, &textureSDR);

    D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc = {};
    shaderResourceViewDesc.Format = textureDesc.Format;
    shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
    shaderResourceViewDesc.Texture2D.MipLevels = 1;
    HRESULT h = d3d11Device->CreateShaderResourceView(textureSDR, &shaderResourceViewDesc, &textureSdrSRV);
    assert(SUCCEEDED(h));

    D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc = {};
    renderTargetViewDesc.Format = textureDesc.Format;
    renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    renderTargetViewDesc.Texture2D.MipSlice = 0;
    h = d3d11Device->CreateRenderTargetView(textureSDR, &renderTargetViewDesc, &textureSdrRTV);
    assert(SUCCEEDED(h));

	//ID3D11Texture2D* textureHDR = nullptr;
    //ID3D11ShaderResourceView* textureHdrSRV = nullptr;
    //ID3D11RenderTargetView* textureHdrRTV = nullptr;

    //textureDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
    //d3d11Device->CreateTexture2D(&textureDesc, nullptr, &textureHDR);
	//d3d11Device->CreateShaderResourceView(textureSDR, nullptr, &textureHdrSRV);
	//d3d11Device->CreateRenderTargetView(textureSDR, nullptr, &textureHdrRTV);

    //free(testTextureBytes);

    // Main Loop
    // #12_MainLoop

    FILE* fp = nullptr;
    auto errorcode = fopen_s(&fp, "capture.log.txt", "w");
    assert(errorcode == 0);

    bool isRunning = true;
    while(isRunning)
    {
        MSG msg = {};
        while(PeekMessageW(&msg, 0, 0, 0, PM_REMOVE))
        {
            if(msg.message == WM_QUIT)
                isRunning = false;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        // #13_ResizeEvent
        if(global_windowDidResize)
        {
            d3d11DeviceContext->OMSetRenderTargets(0, 0, 0);
            d3d11FrameBufferView->Release();

            HRESULT res = d3d11SwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
            assert(SUCCEEDED(res));
            
            ID3D11Texture2D* d3d11FrameBuffer;
            res = d3d11SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&d3d11FrameBuffer);
            assert(SUCCEEDED(res));

            res = d3d11Device->CreateRenderTargetView(d3d11FrameBuffer, NULL,
                                                     &d3d11FrameBufferView);
            assert(SUCCEEDED(res));
            d3d11FrameBuffer->Release();

            global_windowDidResize = false;
        }

        // #22_AquireNextFrame
		IDXGIResource* desktopResource = NULL;
		DXGI_OUTDUPL_FRAME_INFO FrameInfo;
		ID3D11Texture2D* sourceTexture = nullptr;
		ID3D11Texture2D* targetTexture = nullptr;
        ID3D11ShaderResourceView* targetSRV = nullptr;
        ID3D11RenderTargetView* targetRTV = nullptr;

		HRESULT h = outputDup->AcquireNextFrame(50, &FrameInfo, &desktopResource);
		if (FAILED(h))
		{
			OutputDebugStringA("Cannot Acquire Next Frame, Reason: ");
			switch (h) {
			case DXGI_ERROR_WAIT_TIMEOUT:
				OutputDebugStringA("AcquireNextFrame failed. DXGI_ERROR_WAIT_TIMEOUT\n");
				break;
			case DXGI_ERROR_ACCESS_LOST:
				OutputDebugStringA("AcquireNextFrame failed. DXGI_ERROR_ACCESS_LOST\n");
				break;
			default:
				OutputDebugStringA("AcquireNextFrame failed.\n");
				break;
			}
            continue;
		}
        
        BOOL v = (FrameInfo.PointerPosition.Visible);
        int64_t m = FrameInfo.LastMouseUpdateTime.QuadPart;

        char buff[256];
        //sprintf_s(buff, sizeof(buff), "mouseUpdateTime=%lld, PointerPosition.Visible=%d\n", m, v);
        //OutputDebugStringA(buff);

        // #23_GrabDesktopImage
		h = desktopResource->QueryInterface(IID_PPV_ARGS(&sourceTexture));
        desktopResource->Release();
        desktopResource = nullptr;

		D3D11_TEXTURE2D_DESC srcTexDesc;
		sourceTexture->GetDesc(&srcTexDesc);

		g_deskWidth = srcTexDesc.Width;
		g_deskHeight = srcTexDesc.Height;

        UINT format = (UINT)srcTexDesc.Format;
        sprintf_s(buff, sizeof(buff), "DesktopImage Width=%d, Height=%d Format=%d\n", g_deskWidth, g_deskHeight, format);
		OutputDebugStringA(buff);

        fprintf(fp, "%s", buff);

        targetTexture = textureSDR;
        targetSRV = textureSdrSRV;
        targetRTV = textureSdrRTV;

        ID3D11ShaderResourceView* sourceSRV = nullptr;
        if (sourceTexture)
        {
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = srcTexDesc.Format;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = 1;

            h = d3d11Device->CreateShaderResourceView(sourceTexture, &srvDesc, &sourceSRV);
            assert(SUCCEEDED(h));
        }
        else
            OutputDebugStringA("No source texture available!\n");

        /*
		const bool bSameSize = (srcTexDesc.Width == texWidth && srcTexDesc.Height == texHeight);
        if (bSameSize)
        {
			d3d11DeviceContext->CopyResource(targetTexture, sourceTexture);
		}
        else
        {
            OutputDebugStringA("Different Size!");
            fprintf(fp, "Different Size! ");
        }
        */
        
        
        // #31_ClearRTV
        FLOAT backgroundColor2[4] = { 1.f, 0.f, 0.0f, 1.0f };
        d3d11DeviceContext->ClearRenderTargetView(targetRTV, backgroundColor2);

        // #Draw Source to Target

        // #32_SetPipeline
        
        D3D11_VIEWPORT viewport = { 0.0f, 0.0f, texWidth, texHeight, 0.0f, 1.0f };
        d3d11DeviceContext->RSSetViewports(1, &viewport);

        d3d11DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        d3d11DeviceContext->IASetInputLayout(inputLayout);
        d3d11DeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

        d3d11DeviceContext->VSSetShader(vertexShader, nullptr, 0);
        d3d11DeviceContext->PSSetShader(pixelShader, nullptr, 0);

        d3d11DeviceContext->PSSetSamplers(0, 1, &samplerState);

        d3d11DeviceContext->PSSetShaderResources(0, 1, &sourceSRV);
        d3d11DeviceContext->OMSetRenderTargets(1, &targetRTV, nullptr);

        d3d11DeviceContext->Draw(numVerts, 0);

        d3d11DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
        //d3d11DeviceContext->PSSetShaderResources(0, 0, nullptr);

        //#33_DrawCall
		
        FLOAT backgroundColor[4] = { 0.1f, 0.2f, 0.6f, 1.0f };
		d3d11DeviceContext->ClearRenderTargetView(d3d11FrameBufferView, backgroundColor);

        RECT winRect = {};
		GetClientRect(hwnd, &winRect);

        viewport.Width = winRect.right - winRect.left;
        viewport.Height = winRect.bottom - winRect.top;

        d3d11DeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
        d3d11DeviceContext->RSSetViewports(1, &viewport);

        d3d11DeviceContext->PSSetShaderResources(0, 1, &targetSRV);
        d3d11DeviceContext->OMSetRenderTargets(1, &d3d11FrameBufferView, nullptr);
        d3d11DeviceContext->Draw(numVerts, 0);

        d3d11SwapChain->Present(0, 0);

		d3d11DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
		d3d11DeviceContext->PSSetShaderResources(0, 0, nullptr);

        sourceSRV->Release();
        sourceTexture->Release();

        outputDup->ReleaseFrame();
    }

    fclose(fp);
    return 0;
}
