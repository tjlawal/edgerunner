// Windows
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "kernel32")

#include <algorithm>
#include <cassert>
#include <chrono>

#if defined(min)
	#undef min
#endif

#if defined(max)
	#undef max
#endif

// Windows runtime library. Needed for ComPtr<> template class. YUCK
#include <wrl.h>

// DirectX 12 liblary
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

// DirectX extensions library
#include "third_party/directx/d3dx12.h" 

#pragma comment(lib, "d3d12")
#pragma comment(lib, "dxgi")
// #pragma comment(lib, "dxguid.lib")

using namespace Microsoft::WRL;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;


const u8 g_num_frames = 3; // number of swap chain back buffers
const bool g_warp = false; // WARP adapter 
u32 g_client_width = 1200;
u32 g_client_height = 800;
bool g_is_initialized = false;
HWND g_hwnd;
RECT g_window_rect;

// Directx 12 objects
ComPtr<ID3D12Device2> g_device;
ComPtr<ID3D12CommandQueue> g_command_queue;
ComPtr<IDXGISwapChain4> g_swap_chain;
ComPtr<ID3D12Resource> g_backbuffers[g_num_frames];
ComPtr<ID3D12GraphicsCommandList> g_command_list;
ComPtr<ID3D12CommandAllocator> g_command_allocator[g_num_frames];
ComPtr<ID3D12DescriptorHeap> g_rtv_descriptor_heap;
u32 g_rtv_descriptor_size;
u32 g_current_backbuffer_index;

// Synchronization objects
ComPtr<ID3D12Fence> g_fence;
u64 g_fence_val = 0;
u64 g_frame_fence_val[g_num_frames] = {};
HANDLE g_fence_event;

// V-Sync is enable by default`
bool g_vsync = true;
bool g_tearing_supported = false;

// We are in window mode by default
bool g_fullscreen = false;

LRESULT CALLBACK wnd_proc(HWND, UINT, WPARAM, LPARAM);

inline void throw_if_failed(HRESULT hr) {
  if(FAILED(hr)) {
    throw std::exception();
  }
}

void enable_debug_layer() {
 #if defined(_DEBUG)
  ComPtr<ID3D12Debug> debug_interface;
  throw_if_failed(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface)));
	debug_interface->EnableDebugLayer();
 #endif
}

void register_window_class(HINSTANCE hInstance, const wchar_t *class_name) {
	WNDCLASSEXW wnd_class = {};
	wnd_class.cbSize = sizeof(WNDCLASSEXW);
	wnd_class.style = CS_HREDRAW | CS_VREDRAW;
	wnd_class.lpfnWndProc = &wnd_proc;
	wnd_class.cbClsExtra = 0;
	wnd_class.cbWndExtra = 0;
	wnd_class.hInstance = hInstance;
	wnd_class.hIcon = ::LoadIcon(hInstance, NULL);
	wnd_class.hCursor = ::LoadCursor(NULL, IDC_ARROW);
	wnd_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wnd_class.lpszMenuName = NULL;
	wnd_class.lpszClassName = class_name;
	wnd_class.hIconSm = ::LoadIcon(hInstance, NULL);

	static HRESULT hr = ::RegisterClassExW(&wnd_class);
	assert(SUCCEEDED(hr));
}

HWND create_window(const wchar_t *class_name, HINSTANCE hInstance, const wchar_t *window_title, u32 width, u32 height) {
	int screen_width = ::GetSystemMetrics(SM_CXSCREEN);
	int screen_height = ::GetSystemMetrics(SM_CYSCREEN);

	RECT window_rect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
	::AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, FALSE);

	int window_width = window_rect.right - window_rect.left;
	int window_height = window_rect.bottom - window_rect.top;

	// Center window within screen
	int window_x = std::max<int>(0, (screen_width - window_width) / 2);
	int window_y = std::max<int>(0, (screen_height - window_height) / 2);

	HWND hwnd = ::CreateWindowExW(NULL, class_name, window_title, WS_OVERLAPPEDWINDOW, window_x, window_y, window_width, window_height,
																NULL, NULL, hInstance, nullptr);

	assert(hwnd && "Failed to create window");
	return hwnd;
}

ComPtr<IDXGIAdapter4> get_adapter(bool use_warp) {
	ComPtr<IDXGIFactory4> dxgi_factory;
	u32 factory_flags = 0;
	#if defined(_DEBUG)
		factory_flags = DXGI_CREATE_FACTORY_DEBUG;
	#endif

	throw_if_failed(CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&dxgi_factory)));
	ComPtr<IDXGIAdapter1> dxgi_adapter_1;
	ComPtr<IDXGIAdapter4> dxgi_adapter_4;

	if(use_warp) {
		throw_if_failed(dxgi_factory->EnumWarpAdapter(IID_PPV_ARGS(&dxgi_adapter_1)));
		throw_if_failed(dxgi_adapter_1.As(&dxgi_adapter_4));
	} else {
		SIZE_T max_dedicated_video_memory = 0;
		for(u32 i = 0; dxgi_factory->EnumAdapters1(i, &dxgi_adapter_1) 
				!= DXGI_ERROR_NOT_FOUND; ++i) {
			DXGI_ADAPTER_DESC1 dxgi_adapter_desc1;
			dxgi_adapter_1->GetDesc1(&dxgi_adapter_desc1);

			// check to see if the adapter can create a D3D12 device withouth actually creating
			// it. The adapter with the largest dedicated video memory is favoured.
			if((dxgi_adapter_desc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 && 
				SUCCEEDED(D3D12CreateDevice(dxgi_adapter_1.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) 
				&& dxgi_adapter_desc1.DedicatedVideoMemory > max_dedicated_video_memory) {
				max_dedicated_video_memory = dxgi_adapter_desc1.DedicatedVideoMemory;
				throw_if_failed(dxgi_adapter_1.As(&dxgi_adapter_4));
			}
		}
	}
	return dxgi_adapter_4;
}

ComPtr<ID3D12Device2> create_device(ComPtr<IDXGIAdapter4> adapter) {
	ComPtr<ID3D12Device2> d3d12_device_2;
	throw_if_failed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12_device_2)));

	#if defined(_DEBUG)
		ComPtr<ID3D12InfoQueue> p_info_queue;
		if(SUCCEEDED(d3d12_device_2.As(&p_info_queue))) {
			p_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
			p_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
			p_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

			// supress categories of messages
			D3D12_MESSAGE_CATEGORY categories[] = {};

			// suppress messages based on their severity level
			D3D12_MESSAGE_SEVERITY severities[] = {
				D3D12_MESSAGE_SEVERITY_INFO,
			};

			// Suppress individual messages by ID
			D3D12_MESSAGE_ID deny_ids[] = {
				CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
			};
			
			D3D12_INFO_QUEUE_FILTER filter = {};
			filter.DenyList.NumSeverities = _countof(severities);
			filter.DenyList.pSereverityList = severities;
			filter.DenyList.NumIDs = _countof(deny_ids);
			filter.DenyList.pIDList = deny_ids;

			throw_if_failed(p_info_queue->PushStorageFilter(&filter));
		}
	#endif

	return d3d12_device_2;
}

ComPtr<ID3D12CommandQueue> create_command_queue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type) {
	ComPtr<ID3D12CommandQueue> command_queue;

	D3D12_COMMAND_QUEUE_DESC command_queue_desc = {};
	command_queue_desc.Type = type;
	command_queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	command_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	command_queue_desc.NodeMask = 0;

	throw_if_failed(device->CreateCommandQueue(&command_queue_desc, IID_PPV_ARGS(&command_queue)));
	return command_queue;

}

bool check_tearing_support() {
	bool allow_tearing = FALSE;

	// Instead of creating a DXGI 1.5 factory interface directly, create the 
	// DXGI 1.4 interface and query for the 1.5 interface. This is to enable the
	// graphics debugging tools which will not support the 1.5 factory until a future update

	ComPtr<IDXGIFactory4> factory_4;
	if(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory_4)))) {
		ComPtr<IDXGIFactory5> factory_5;
		if(SUCCEEDED(factory_4.As(&factory_5))) {
			if(FAILED(factory_5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(allow_tearing)))) {
				allow_tearing = FALSE;
			}
		}
	}	
	return allow_tearing == TRUE;
}

ComPtr<IDXGISwapChain4> create_swap_chain(HWND hwnd, ComPtr<ID3D12CommandQueue> command_queue, u32 width, u32 height, u32 buffer_count) {
	ComPtr<IDXGISwapChain4> dxgi_swapchain4;
	ComPtr<IDXGIFactory4> dxgi_factory4;
	u32 factory_flags = 0;

	#if defined(_DEBUG)
		factory_flags = DXGI_CREATE_FACTORY_DEBUG;
	#endif

	throw_if_failed(CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&dxgi_factory4)));
	DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {};
	swapchain_desc.Width = width;
	swapchain_desc.Height = height;
	swapchain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchain_desc.Stereo = FALSE;
	swapchain_desc.SampleDesc = {1, 0};
	swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchain_desc.BufferCount = buffer_count;
	swapchain_desc.Scaling = DXGI_SCALING_STRETCH;
	swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapchain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

	// Always allow tearing support if its available
	swapchain_desc.Flags = check_tearing_support() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	ComPtr<IDXGISwapChain1> swapchain_1;
	throw_if_failed(dxgi_factory4->CreateSwapChainForHwnd(
		command_queue.Get(), hwnd, &swapchain_desc, nullptr, nullptr, &swapchain_1));

	// Disbale Alt+Enter to toggle fullscreen, I'll handle it manually.
	throw_if_failed(dxgi_factory4->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
	throw_if_failed(swapchain_1.As(&dxgi_swapchain4));
	return dxgi_swapchain4;
} 

ComPtr<ID3D12DescriptorHeap> create_descriptor_heap(ComPtr<ID3D12Device2> device, D3D12_DESCRIPTOR_HEAP_TYPE heap_type, u32 descriptors) {
	ComPtr<ID3D12DescriptorHeap> heap_descriptor;
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = descriptors;
	desc.Type = heap_type;

	throw_if_failed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap_descriptor)));
	return heap_descriptor;
}

void update_render_target_views(ComPtr<ID3D12Device2> device, ComPtr<IDXGISwapChain4>swapchain, ComPtr<ID3D12DescriptorHeap> heap_descriptor) {
	auto rtv_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(heap_descriptor->GetCPUDescriptorHandleForHeapStart());

	for(int i = 0; i < g_num_frames; ++i) {
		ComPtr<ID3D12Resource> backbuffer;
    throw_if_failed(swapchain->GetBuffer(i, IID_PPV_ARGS(&backbuffer)));
    device->CreateRenderTargetView(backbuffer.Get(), nullptr, rtv_handle);
    g_backbuffers[i] = backbuffer;
    rtv_handle.Offset(rtv_descriptor_size);
	}
}

ComPtr<ID3D12CommandAllocator> create_command_allocator(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type) {
  ComPtr<ID3D12CommandAllocator> command_allocator;
  throw_if_failed(device->CreateCommandAllocator(type, IID_PPV_ARGS(&command_allocator)));

  return command_allocator;
}

ComPtr<ID3D12GraphicsCommandList> create_command_list(ComPtr<ID3D12Device2> device, ComPtr<ID3D12CommandAllocator> command_allocator, D3D12_COMMAND_LIST_TYPE type) {
  ComPtr<ID3D12GraphicsCommandList> command_list;

  throw_if_failed(device->CreateCommandList(0, type, command_allocator.Get(), nullptr,
                                             IID_PPV_ARGS(&command_list)));

  throw_if_failed(command_list->Close());
  return command_list;
}

ComPtr<ID3D12Fence> create_fence(ComPtr<ID3D12Device2> device) {
	ComPtr<ID3D12Fence> fence;
	throw_if_failed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
	return fence;
}

// Create an event handle 
HANDLE create_event_handle() {
	HANDLE fence_evt;
	fence_evt = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fence_evt && "Failde to create fence event");
	return fence_evt;
}

u64 signal(ComPtr<ID3D12CommandQueue> command_queue, ComPtr<ID3D12Fence> fence, u64 &fence_val) {
	u64 fence_val_for_signal = ++fence_val;
	throw_if_failed(command_queue->Signal(fence.Get(), fence_val_for_signal));
	return fence_val_for_signal;
}

void wait_for_fence_value(ComPtr<ID3D12Fence> fence, u64 fence_val, HANDLE fence_evt, std::chrono::milliseconds duration = std::chrono::milliseconds::max()) {
	if(fence->GetCompletedValue() < fence_val) {
		throw_if_failed(fence->SetEventOnCompletion(fence_val, fence_evt));
		::WaitForSingleObject(fence_evt, static_cast<DWORD>(duration.count()));
	}
}

void flush(ComPtr<ID3D12CommandQueue> command_queue, ComPtr<ID3D12Fence> fence, u64 &fence_val, HANDLE fence_evt) {
	u64 fence_val_for_signal = signal(command_queue, fence, fence_val);
	wait_for_fence_value(fence, fence_val_for_signal, fence_evt);
}

void update() {
	static u64 frame_counter = 0;
	static f64 elapsed_seconds = 0.0;
	static std::chrono::high_resolution_clock clock;
	static auto t0 = clock.now();

	frame_counter++;
	auto t1 = clock.now();
	auto dt = t1 - t0;
	t0 = t1;

	elapsed_seconds += dt.count() * 1e-9;
	if(elapsed_seconds > 1.0) {
		char buf[500];
		auto fps = frame_counter / elapsed_seconds;
		sprintf_s(buf, 500, "FPS: %f\n", fps);
		OutputDebugString(buf);
		frame_counter = 0;
		elapsed_seconds = 0.0;
	}
}

void render() {
	auto command_allocator = g_command_allocator[g_current_backbuffer_index];
	auto back_buffer = g_backbuffers[g_current_backbuffer_index];
	command_allocator->Reset();
	g_command_list->Reset(command_allocator.Get(), nullptr);

	// Clear the render target, it must be transitioned to RENDER_TARGET state before
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(back_buffer.Get(),
																																						D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		g_command_list->ResourceBarrier(1, &barrier);
	
		f32 clear_colour[] = { 0.4f, 0.6f, 0.9f, 1.0f };
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(g_rtv_descriptor_heap->GetCPUDescriptorHandleForHeapStart(), g_current_backbuffer_index, g_rtv_descriptor_size);
		g_command_list->ClearRenderTargetView(rtv, clear_colour, 0, nullptr);
	}

	// Present
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(back_buffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		g_command_list->ResourceBarrier(1, &barrier);
		throw_if_failed(g_command_list->Close());
		ID3D12CommandList* const command_lists[] = {
			g_command_list.Get()
		};

		g_command_queue->ExecuteCommandLists(_countof(command_lists), command_lists);
		
		UINT sync_interval = g_vsync ? 1 : 0;
		UINT present_flags = g_tearing_supported && !g_vsync ? DXGI_PRESENT_ALLOW_TEARING : 0;
		throw_if_failed(g_swap_chain->Present(sync_interval, present_flags));
		g_frame_fence_val[g_current_backbuffer_index] = signal(g_command_queue, g_fence, g_fence_val);
		g_current_backbuffer_index = g_swap_chain->GetCurrentBackBufferIndex();
		wait_for_fence_value(g_fence, g_frame_fence_val[g_current_backbuffer_index], g_fence_event);
	}
}

void resize(u32 width, u32 height) {
	if((g_client_width != width) || (g_client_height != height)) {
		// Don't allow 0 size swap chain back buffers
		g_client_width = std::max(1U, width);
		g_client_width = std::max(1U, height);

		// Flush the GPU queue to make sure the swap chain's back buffers 
		// are not being referenced by an in-flight command list.
		flush(g_command_queue, g_fence, g_fence_val, g_fence_event);

		for(s32 i = 0; i < g_num_frames; ++i) {
			// Any references to the back buffer must be released before
			// the swap chain can be resized.
			g_backbuffers[i].Reset();
			g_frame_fence_val[i] = g_frame_fence_val[g_current_backbuffer_index];
		}

		// Re-query the swap chain description to ensure the same color format
		// and swap chain flags are used to recreate the swap chain buffers
		DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};
		throw_if_failed(g_swap_chain->GetDesc(&swap_chain_desc));
		throw_if_failed(g_swap_chain->ResizeBuffers(g_num_frames, g_client_width, g_client_height, swap_chain_desc.BufferDesc.Format, swap_chain_desc.Flags));
		g_current_backbuffer_index = g_swap_chain->GetCurrentBackBufferIndex();
		update_render_target_views(g_device, g_swap_chain, g_rtv_descriptor_heap);
	}
}

void set_fullscreen(bool fullscreen) {
	if(g_fullscreen != fullscreen) {
		g_fullscreen = fullscreen;

		if(g_fullscreen) { 
			// Switch to fullscreen

			// Store the current window dimensions so that we can restore when switching out of fullscreen.
			::GetWindowRect(g_hwnd, &g_window_rect);
			
			// Set window style to a borderless window so the client's 
			// area fills entire screen 
			UINT window_style = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
			::SetWindowLongW(g_hwnd, GWL_STYLE, window_style);

			// Query the name of the nearest display device for the window.
			// This is required to set the fullscreen dimensions for the window when using 
			// a multi-monitor setup.
			HMONITOR hmonitor = ::MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST);
			MONITORINFOEX monitor_info = {};
			monitor_info.cbSize = sizeof(MONITORINFOEX);
			::GetMonitorInfo(hmonitor, &monitor_info);

			::SetWindowPos(g_hwnd, HWND_TOP, monitor_info.rcMonitor.left, monitor_info.rcMonitor.top,
										 monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
										 monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
										 SWP_FRAMECHANGED | SWP_NOACTIVATE);
			::ShowWindow(g_hwnd, SW_MAXIMIZE);
		}
		else {
			// Restore back to windowed mode
			::SetWindowLong(g_hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
			::SetWindowPos(g_hwnd, HWND_NOTOPMOST, g_window_rect.left, g_window_rect.top,
										 g_window_rect.right - g_window_rect.left,
										 g_window_rect.bottom - g_window_rect.top,
										 SWP_FRAMECHANGED | SWP_NOACTIVATE);
			::ShowWindow(g_hwnd, SW_NORMAL);
		}
	}
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if(g_is_initialized) {
		switch(msg) {
			case WM_PAINT: {
				update();
				render();
			} break;

			case WM_KEYDOWN:
			case WM_SYSKEYDOWN: {
				bool alt_key = (::GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
				bool ctrl_key = (::GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
				switch(wParam) {
					case('v'): { g_vsync = !g_vsync; } break;
					case(VK_ESCAPE): ::PostQuitMessage(0);break;
					case('f'): {
						if(ctrl_key) set_fullscreen(!g_fullscreen);
					} break;
				}
			} break;

			case WM_SYSCHAR: {} break;
				
			case WM_SIZE: {
				RECT client_rect = {};
				::GetClientRect(g_hwnd, &client_rect);

				s32 width = client_rect.right - client_rect.left;
				s32 height = client_rect.bottom - client_rect.top;

				resize(width, height);
			} break;

			case WM_DESTROY: { ::PostQuitMessage(0); } break;

			default: {
				return ::DefWindowProcW(hwnd, msg, wParam, lParam);
			}
		}
	}

	else {
		return ::DefWindowProcW(hwnd, msg, wParam, lParam);
	}

	return 0;
}




int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLne, int nCmdShow) {
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	const wchar_t *window_class_name = L"edge-runner-window-class";
	enable_debug_layer();
	g_tearing_supported = check_tearing_support();
	register_window_class(hInstance, window_class_name);

	g_hwnd = create_window(window_class_name, hInstance, L"Edge Runner", g_client_width, g_client_height);

	// Initialize global window rect variable
	::GetWindowRect(g_hwnd, &g_window_rect);

	// Initialize DirectX
	ComPtr<IDXGIAdapter4> dxgiadapter_4 = get_adapter(g_warp);
	g_device = create_device(dxgiadapter_4);
	g_command_queue = create_command_queue(g_device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	g_swap_chain = create_swap_chain(g_hwnd, g_command_queue, g_client_width, g_client_height, g_num_frames);
	g_current_backbuffer_index = g_swap_chain->GetCurrentBackBufferIndex();
	g_rtv_descriptor_heap = create_descriptor_heap(g_device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, g_num_frames);
	g_rtv_descriptor_size = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	update_render_target_views(g_device, g_swap_chain, g_rtv_descriptor_heap);

	// Command list and allocators
	for(s32 i = 0; i < g_num_frames; ++i) {
		g_command_allocator[i] = create_command_allocator(g_device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	}
	g_command_list = create_command_list(g_device, g_command_allocator[g_current_backbuffer_index], D3D12_COMMAND_LIST_TYPE_DIRECT);

	g_fence = create_fence(g_device);
	g_fence_event = create_event_handle();

	// Everything is already setup, now start showing window

	g_is_initialized = true;
	::ShowWindow(g_hwnd, SW_SHOW);

	MSG msg = {};
	while(msg.message != WM_QUIT) {
		if(::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)){
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
	}

	// Make sure the command queue as finished all commands before closing
	flush(g_command_queue, g_fence, g_fence_val, g_fence_event);
	::CloseHandle(g_fence_event);


	return 0;
}
