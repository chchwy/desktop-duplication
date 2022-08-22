#include "pch.h"

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")

D3D_FEATURE_LEVEL pFeatureLevel;
ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_immediateContext = nullptr;
IDXGIOutputDuplication* g_outputDup = nullptr;

void Init()
{
    HRESULT h = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        nullptr,
		0,
        D3D11_SDK_VERSION,
		&g_device,
		&pFeatureLevel,
		&g_immediateContext
	);

    if (h != S_OK)
        assert(false);

    if (pFeatureLevel != D3D_FEATURE_LEVEL_11_0)
        assert(false);

	IDXGIDevice* pDXGIDevice = nullptr;
	h = g_device->QueryInterface(__uuidof(IDXGIDevice), (void**)&pDXGIDevice);
    if (h != S_OK) {
        assert(false);
    }

	IDXGIAdapter* pAdapter = nullptr;
    h = pDXGIDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&pAdapter);
    if (h != S_OK) {
        assert(false);
    }

    UINT i = 0;
	IDXGIOutput* output = nullptr;
    IDXGIOutput1* output1 = nullptr;

	if (pAdapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_OUTPUT_DESC desc;
		output->GetDesc(&desc);

		std::wstring text = L"***Output: ";
		text += desc.DeviceName;
		text += L"\n";
		OutputDebugString(text.c_str());

        HRESULT h2 = output->QueryInterface(__uuidof(IDXGIOutput1), (void**)&output1);

		output->Release();
		++i;

        //OutputDebugStringA(std::to_string(i).c_str());
	}

    if (h != S_OK) assert(false);

    output1->DuplicateOutput(g_device, &g_outputDup);
    output1->Release();

	DXGI_OUTDUPL_DESC odd;
    g_outputDup->GetDesc(&odd);

    std::ostringstream sout;
    sout << "Width :" << odd.ModeDesc.Width << "\n";
    sout << "Height:" << odd.ModeDesc.Height << "\n";
    OutputDebugStringA(sout.str().c_str());
}


void Shutdown()
{
    g_device->Release();
    g_immediateContext->Release();
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;
    switch (msg)
    {
    case WM_KEYDOWN:
    {
        if (wparam == VK_ESCAPE)
        {
            Shutdown();
            DestroyWindow(hwnd);
        }
        break;
    }
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        break;
    }
    default:
        result = DefWindowProcW(hwnd, msg, wparam, lparam);
    }
    return result;
}

void Render()
{
	IDXGIResource* desktopResource = NULL;
	DXGI_OUTDUPL_FRAME_INFO FrameInfo;

	HRESULT h = g_outputDup->AcquireNextFrame(100, &FrameInfo, &desktopResource);
    if (h != S_OK)
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
        g_outputDup->ReleaseFrame();
        return;
    }
    BOOL v = (FrameInfo.PointerPosition.Visible);
    int64_t m = FrameInfo.LastMouseUpdateTime.QuadPart;

    char buff[256];
    sprintf_s(buff, sizeof(buff), "mouseUpdateTime=%lld, PointerPosition.Visible=%d\n", m, v);
    OutputDebugStringA(buff);

    g_outputDup->ReleaseFrame();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    // Open a window
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

        if (!RegisterClassExW(&winClass))
        {
            MessageBoxA(0, "RegisterClassEx failed", "Fatal Error", MB_OK);
            return GetLastError();
        }

        RECT initialRect = { 0, 0, 640, 480 };
        AdjustWindowRectEx(&initialRect, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_OVERLAPPEDWINDOW);
        LONG initialWidth = initialRect.right - initialRect.left;
        LONG initialHeight = initialRect.bottom - initialRect.top;

        hwnd = CreateWindowExW(WS_EX_OVERLAPPEDWINDOW,
                               winClass.lpszClassName,
                               L"Win32 Window",
                               WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                               CW_USEDEFAULT, CW_USEDEFAULT,
                               initialWidth,
                               initialHeight,
                               0, 0, hInstance, 0);

        if (!hwnd)
        {
            MessageBoxA(0, "CreateWindowEx failed", "Fatal Error", MB_OK);
            return GetLastError();
        }
    }

    Init();

    bool isRunning = true;
    while (isRunning)
    {
        MSG message = {};
        while (PeekMessageW(&message, 0, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                isRunning = false;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        Render();
        Sleep(1);
    }

    return 0;
}
