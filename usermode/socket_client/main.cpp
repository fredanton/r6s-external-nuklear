#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#include "driver.h"
#include <iostream>
#include <TlHelp32.h>

#define WINDOW_WIDTH 300
#define WINDOW_HEIGHT 400

#include "nuklear/nuklear_d3d11.h"
#include "manager.h"

static IDXGISwapChain * swap_chain;
static ID3D11Device* device;
static ID3D11DeviceContext* context;
static ID3D11RenderTargetView* rt_view;

static uint32_t
find_process_by_id(const char* name)
{
	const auto snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snap == INVALID_HANDLE_VALUE) {
		return 0;
	}

	PROCESSENTRY32 proc_entry;
	proc_entry.dwSize = sizeof proc_entry;

	auto found_process = FALSE;
	if (!!Process32First(snap, &proc_entry)) {
		do {
			if (name == proc_entry.szExeFile) {
				found_process = TRUE;
				break;
			}
		} while (!!Process32Next(snap, &proc_entry));
	}

	CloseHandle(snap);
	return found_process
		? proc_entry.th32ProcessID
		: 0;
}

static void
set_swap_chain_size(int width, int height)
{
	ID3D11Texture2D* back_buffer;
	D3D11_RENDER_TARGET_VIEW_DESC desc;
	HRESULT hr;

	if (rt_view)
		rt_view->Release();

	context->OMSetRenderTargets(0, NULL, NULL);

	hr = swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET || hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR)
	{
		/* to recover from this, you'll need to recreate device and all the resources */
		MessageBoxW(NULL, L"DXGI device is removed or reset!", L"Error", 0);
		exit(0);
	}
	assert(SUCCEEDED(hr));

	memset(&desc, 0, sizeof(desc));
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

	hr = swap_chain->GetBuffer(0, IID_ID3D11Texture2D, (void**)& back_buffer);
	assert(SUCCEEDED(hr));

	hr = device->CreateRenderTargetView((ID3D11Resource*)back_buffer, &desc, &rt_view);
	assert(SUCCEEDED(hr));

	back_buffer->Release();
}

static LRESULT CALLBACK
WindowProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_SIZE:
		if (swap_chain)
		{
			int width = LOWORD(lparam);
			int height = HIWORD(lparam);
			set_swap_chain_size(width, height);
			nk_d3d11_resize(context, width, height);
		}
		break;
	}

	if (nk_d3d11_handle_event(wnd, msg, wparam, lparam))
		return 0;

	return DefWindowProcW(wnd, msg, wparam, lparam);
}

int main()
{
	driver::initialize();

	manager::m_connection = (SOCKET)driver::connect;
	if (manager::m_connection == INVALID_SOCKET)
	{
		std::cout << "Connection failed.\n";
		return -1;
	}

	driver::clean_piddbcachetable(manager::m_connection);
	driver::clean_mmunloadeddrivers(manager::m_connection);

	manager::m_pid = find_process_by_id("notepad.exe");
	manager::m_base = driver::get_process_base_address(manager::m_connection, manager::m_pid);

	struct nk_context* ctx;
	struct nk_colorf bg;

	WNDCLASSW wc;
	RECT rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
	DWORD style = WS_OVERLAPPEDWINDOW;
	DWORD exstyle = WS_EX_APPWINDOW;
	HWND wnd;
	int running = 1;
	HRESULT hr;
	D3D_FEATURE_LEVEL feature_level;
	DXGI_SWAP_CHAIN_DESC swap_chain_desc;

	memset(&wc, 0, sizeof(wc));
	wc.style = CS_DBLCLKS;
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = GetModuleHandleW(0);
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszClassName = L"NuklearWindowClass";
	RegisterClassW(&wc);

	AdjustWindowRectEx(&rect, style, FALSE, exstyle);

	wnd = CreateWindowExW(exstyle, wc.lpszClassName, L"alxbrn external",
		style | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
		rect.right - rect.left, rect.bottom - rect.top,
		NULL, NULL, wc.hInstance, NULL);

	memset(&swap_chain_desc, 0, sizeof(swap_chain_desc));
	swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
	swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
	swap_chain_desc.SampleDesc.Count = 1;
	swap_chain_desc.SampleDesc.Quality = 0;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.BufferCount = 1;
	swap_chain_desc.OutputWindow = wnd;
	swap_chain_desc.Windowed = TRUE;
	swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	swap_chain_desc.Flags = 0;
	if (FAILED(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE,
		NULL, 0, NULL, 0, D3D11_SDK_VERSION, &swap_chain_desc,
		&swap_chain, &device, &feature_level, &context)))
	{
		hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_WARP,
			NULL, 0, NULL, 0, D3D11_SDK_VERSION, &swap_chain_desc,
			&swap_chain, &device, &feature_level, &context);
		assert(SUCCEEDED(hr));
	}
	set_swap_chain_size(WINDOW_WIDTH, WINDOW_HEIGHT);

	ctx = nk_d3d11_init(device, WINDOW_WIDTH, WINDOW_HEIGHT, MAX_VERTEX_BUFFER, MAX_INDEX_BUFFER);

	{
		struct nk_font_atlas* atlas;
		nk_d3d11_font_stash_begin(&atlas);
		nk_d3d11_font_stash_end();
	}

	bg.r = 0.10f, bg.g = 0.18f, bg.b = 0.24f, bg.a = 1.0f;
	while (running)
	{
		MSG msg;
		nk_input_begin(ctx);
		while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
				running = 0;
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		nk_input_end(ctx);

		if (nk_begin(ctx, "Demo", nk_rect(0, 0, 300, 400), NULL))
		{
			enum { EASY, HARD };
			static int op = EASY;
			static int property = 20;

			nk_layout_row_static(ctx, 30, 80, 1);
			if (nk_button_label(ctx, "button"))
				fprintf(stdout, "button pressed\n");
			nk_layout_row_dynamic(ctx, 30, 2);
			if (nk_option_label(ctx, "easy", op == EASY)) op = EASY;
			if (nk_option_label(ctx, "hard", op == HARD)) op = HARD;
			nk_layout_row_dynamic(ctx, 22, 1);
			nk_property_int(ctx, "Compression:", 0, &property, 100, 10, 1);

			nk_layout_row_dynamic(ctx, 20, 1);
			nk_label(ctx, "background:", NK_TEXT_LEFT);
			nk_layout_row_dynamic(ctx, 25, 1);
			if (nk_combo_begin_color(ctx, nk_rgb_cf(bg), nk_vec2(nk_widget_width(ctx), 400))) {
				nk_layout_row_dynamic(ctx, 120, 1);
				bg = nk_color_picker(ctx, bg, NK_RGBA);
				nk_layout_row_dynamic(ctx, 25, 1);
				bg.r = nk_propertyf(ctx, "#R:", 0, bg.r, 1.0f, 0.01f, 0.005f);
				bg.g = nk_propertyf(ctx, "#G:", 0, bg.g, 1.0f, 0.01f, 0.005f);
				bg.b = nk_propertyf(ctx, "#B:", 0, bg.b, 1.0f, 0.01f, 0.005f);
				bg.a = nk_propertyf(ctx, "#A:", 0, bg.a, 1.0f, 0.01f, 0.005f);
				nk_combo_end(ctx);
			}

			// Cheat related stuff (Example)
			//for (int i = 0; i < entity_count; i++) {
			//	const auto health = driver::read<float>(connection, pid, (base_address + game_manager) + (i * 8));
			//}
		}
		nk_end(ctx);

		/* Draw */
		context->ClearRenderTargetView(rt_view, &bg.r);
		context->OMSetRenderTargets(1, &rt_view, NULL);
		nk_d3d11_render(context, NK_ANTI_ALIASING_ON);
		hr = swap_chain->Present(1, 0);
		if (hr == DXGI_ERROR_DEVICE_RESET || hr == DXGI_ERROR_DEVICE_REMOVED) {
			MessageBoxW(NULL, L"D3D11 device is lost or removed!", L"Error", 0);
			break;
		}
		else if (hr == DXGI_STATUS_OCCLUDED) {
			Sleep(10);
		}
		assert(SUCCEEDED(hr));
	}

	context->ClearState();
	nk_d3d11_shutdown();
	rt_view->Release();
	context->Release();
	device->Release();
	swap_chain->Release();
	UnregisterClassW(wc.lpszClassName, wc.hInstance);

	driver::disconnect(manager::m_connection);
	driver::deinitialize();

	return 0;
}