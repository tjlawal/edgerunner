
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>

#include <iostream>
#include <string>

// Static libs
#pragma comment(lib, "user32")
#pragma comment(lib, "Winmm")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "d3dcompiler")

// Keywords Macros
#define global static
#define internal static
#define local static

// Base Types
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef s8 b8;
typedef s16 b16;
typedef s32 b32;
typedef s64 b64;

typedef float f32;
typedef double f64;

using namespace DirectX;

// For releasing COM objects
template <typename T> inline void SafeRelease(T &ptr) {
  if (ptr != NULL) {
    ptr->Release();
    ptr = NULL;
  }
}

// Globals
HWND g_window_handle 							 = 0;
const LONG g_window_width 				 = 1280;
const LONG g_window_height 				 = 720;
LPCSTR g_window_name 							 = "Edgerunner";
LPCSTR g_window_class_name 				 = "[edgerunner-gfx-class]";
const BOOL g_enable_vsync					 = false;
global b32 g_is_fullscreen 				 = false;

global s16 g_texture_width 				 = 0;
global s16 g_texture_height 			 = 0;
global u32 row_pitch							 = 0;
global u8* g_texture_data = nullptr;


WINDOWPLACEMENT g_last_window_placement;

// Direct3D device and swapchain
ID3D11Device*							g_device          = nullptr;
IDXGISwapChain*						g_swapchain       = nullptr;
ID3D11DeviceContext*			g_device_context  = nullptr;
ID3D11RenderTargetView*		g_framebuffer_rtv = nullptr;			

ID3D11Texture2D*					g_depth_stencil_buffer = nullptr;        
ID3D11DepthStencilView*		g_depth_stencil_view   = nullptr;   
ID3D11DepthStencilState*	g_depth_stencil_state  = nullptr; 

ID3D11SamplerState*				g_sampler_state    = nullptr;
ID3D11RasterizerState*		g_rasterizer_state = nullptr;

D3D11_VIEWPORT 						g_viewport = {};

// Vertex buffer data
ID3D11Buffer*							g_vertex_buffer = nullptr;
ID3D11Buffer*							g_index_buffer  = nullptr;
ID3D11InputLayout*				g_input_layout  = nullptr;
ID3D11ShaderResourceView* g_shader_rsv		= nullptr;

// Shader data
ID3D11PixelShader*				g_pixel_shader  = nullptr;
ID3D11VertexShader*				g_vertex_shader = nullptr;

// Texture
ID3D11Texture2D*					g_texture 			= nullptr;



// Projection related
XMMATRIX g_world_matrix;
XMMATRIX g_view_matrix; 
XMMATRIX g_projection_matrix;

enum ConstantBuffer : u64 {
  ConstantBuffer_Application,
  ConstantBuffer_Frame,
  ConstantBuffer_Object,
  ConstantBuffer_COUNT
};

ID3D11Buffer *g_constant_buffers[ConstantBuffer_COUNT];

//------------------------------------------------------------------------
// DATA
//------------------------------------------------------------------------

struct Vertex {
  XMFLOAT3 position;
  XMFLOAT2 texture;
};

//Vertex g_vertices[3] = {
//	{ XMFLOAT3(1.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
//	{ XMFLOAT3(0.0f, 1.0f, 0.0f),  XMFLOAT2(0.5f, 0.0f) },
//	{ XMFLOAT3(1.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) }
//};

//u16 g_indices[3] = { 0, 1, 2 };

Vertex g_vertices[3] = {
	{ XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) }, // Bottom-left
	{ XMFLOAT3( 0.0f,  1.0f, 0.0f), XMFLOAT2(0.5f, 0.0f) }, // Top-center
	{ XMFLOAT3( 1.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) }  // Bottom-right
};

u16 g_indices[3] = { 0, 1, 2 };


internal b32	window_is_fullscreen(HWND window_handle) {
	//OS_Window* window = w32_window_from_handle(window_handle);
	DWORD window_style = GetWindowLong(window_handle, GWL_STYLE);
	return !(window_style & WS_OVERLAPPEDWINDOW);
}

internal void	window_set_fullscreen(HWND window_handle, b32 fullscreen) {
	//OS_Window* window = w32_window_from_handle(window_handle);
	DWORD window_style = GetWindowLong(window_handle, GWL_STYLE);
	b32 already_fullscreen = window_is_fullscreen(window_handle);
	if(fullscreen) {
		if(!already_fullscreen) {
			GetWindowPlacement(window_handle, &g_last_window_placement);
		}
		MONITORINFO monitor_info = { sizeof(monitor_info) };
		if(GetMonitorInfo(MonitorFromWindow(window_handle, MONITOR_DEFAULTTOPRIMARY), &monitor_info)){
			SetWindowLong(window_handle, GWL_STYLE, window_style & ~WS_OVERLAPPEDWINDOW);
			SetWindowPos(window_handle, HWND_TOP, monitor_info.rcMonitor.left, monitor_info.rcMonitor.top,
									 monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
									 monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top, 
									 SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		}
	} 
	else {
		SetWindowLong(window_handle, GWL_STYLE, window_style | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(window_handle, &g_last_window_placement);
		SetWindowPos(window_handle, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
	}
}


LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  PAINTSTRUCT paintStruct;
  HDC hDC;

  switch (message) {
		case WM_PAINT: {
			hDC = BeginPaint(hwnd, &paintStruct);
			EndPaint(hwnd, &paintStruct);
		} break;
		case WM_DESTROY: {
			PostQuitMessage(0);
		} break;
		case WM_CHAR: {
			switch((TCHAR)wParam) {
				case('q'): {
					PostQuitMessage(0);
				} break;
				case('f'): {
					g_is_fullscreen = !g_is_fullscreen;
					window_set_fullscreen(g_window_handle, g_is_fullscreen); 
				}
			} break;

		}
		default:
			return DefWindowProc(hwnd, message, wParam, lParam);
  }

  return 0;
}

int init_application(HINSTANCE hInstance, int cmdShow) {
  WNDCLASSEX wndClass = {0};
  wndClass.cbSize = sizeof(WNDCLASSEX);
  wndClass.style = CS_HREDRAW | CS_VREDRAW;
  wndClass.lpfnWndProc = &WndProc;
  wndClass.hInstance = hInstance;
  wndClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wndClass.lpszMenuName = nullptr;
  wndClass.lpszClassName = g_window_class_name;

  if (!RegisterClassEx(&wndClass)) { return -1; }

  RECT windowRect = {0, 0, g_window_width, g_window_height};
  AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

  g_window_handle = CreateWindowA(g_window_class_name, g_window_name, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                  windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, nullptr,
                                  nullptr, hInstance, nullptr);

  if (!g_window_handle) { return -1; }

  ShowWindow(g_window_handle, cmdShow);
  UpdateWindow(g_window_handle);

  return 0;
}

int init_directx(HINSTANCE hInstance, BOOL vsync) {
  assert(g_window_handle != 0);

  RECT client_rect;
  GetClientRect(g_window_handle, &client_rect);

  u32 client_width = client_rect.right - client_rect.left;
  u32 client_height = client_rect.bottom - client_rect.top;

  DXGI_SWAP_CHAIN_DESC swapchain_desc = {};
  swapchain_desc.BufferCount = 1;
  swapchain_desc.BufferDesc.Width = client_width;
  swapchain_desc.BufferDesc.Height = client_height;
  swapchain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  // swapchain_desc.BufferDesc.RefreshRate = QueryRefereshRate(client_width, client_height, vsync);
  swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapchain_desc.OutputWindow = g_window_handle;
  swapchain_desc.SampleDesc.Count = 1;
  swapchain_desc.SampleDesc.Quality = 0;
  swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  swapchain_desc.Windowed = true;

  u32 device_flags = 0;
	#if _DEBUG
	device_flags = D3D11_CREATE_DEVICE_DEBUG;
	#endif

  // These are the feature levels that we will accept.
  D3D_FEATURE_LEVEL feature_levels[] = {
		D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, 
		D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0, 
		D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_1
	};
	
  HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 
																						 device_flags, feature_levels,
                                             _countof(feature_levels), D3D11_SDK_VERSION, &swapchain_desc, &g_swapchain,
                                             &g_device, 0, &g_device_context);
  if (FAILED(hr)) {
    return false;
  }

  // Initialize the back buffer of the swapchain and associate with a render target
  {
    ID3D11Texture2D *framebuffer;
    hr = g_swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID *)&framebuffer);
    if (FAILED(hr)) { return -1; }

    hr = g_device->CreateRenderTargetView(framebuffer, nullptr, &g_framebuffer_rtv);
    if (FAILED(hr)) { return -1; }
    SafeRelease(framebuffer);
  }

  // Create the depth buffer for use with the depth/stencil view
  {
    D3D11_TEXTURE2D_DESC depth_stencil_buffer_desc = {};
    depth_stencil_buffer_desc.ArraySize = 1;
    depth_stencil_buffer_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depth_stencil_buffer_desc.CPUAccessFlags = 0;
    depth_stencil_buffer_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depth_stencil_buffer_desc.Width = client_width;
    depth_stencil_buffer_desc.Height = client_height;
    depth_stencil_buffer_desc.MipLevels = 1;
    depth_stencil_buffer_desc.SampleDesc.Count = 1;
    depth_stencil_buffer_desc.SampleDesc.Quality = 0;
    depth_stencil_buffer_desc.Usage = D3D11_USAGE_DEFAULT;

    hr = g_device->CreateTexture2D(&depth_stencil_buffer_desc, nullptr,  &g_depth_stencil_buffer);
    if (FAILED(hr)) { return -1; }

    hr = g_device->CreateDepthStencilView(g_depth_stencil_buffer, nullptr, &g_depth_stencil_view);
    if (FAILED(hr)) { return -1; }
  }

  // Setup depth/stencil state. 
	// This controls how depth-stencil testing is performed by the Output-Merge stage and
  // Rasterizer stage.
  {
    D3D11_DEPTH_STENCIL_DESC depth_stencil_desc = {};
    depth_stencil_desc.DepthEnable = true;
    depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depth_stencil_desc.DepthFunc = D3D11_COMPARISON_LESS;
    depth_stencil_desc.StencilEnable = false;

    hr = g_device->CreateDepthStencilState(&depth_stencil_desc, &g_depth_stencil_state);
    if (FAILED(hr)) { return -1; }
  }

  // Setup the rasterizer state
  {
    D3D11_RASTERIZER_DESC rasterizer_desc = {};
    rasterizer_desc.AntialiasedLineEnable = false;
    //rasterizer_desc.CullMode = D3D11_CULL_BACK;
    rasterizer_desc.CullMode = D3D11_CULL_NONE;
    rasterizer_desc.DepthBias = 0;
    rasterizer_desc.DepthBiasClamp = 0.0f;
    rasterizer_desc.DepthClipEnable = true;
    rasterizer_desc.FillMode = D3D11_FILL_SOLID;
    rasterizer_desc.FrontCounterClockwise = false;
    rasterizer_desc.MultisampleEnable = false;
    rasterizer_desc.ScissorEnable = false;
    rasterizer_desc.SlopeScaledDepthBias = 0.0f;

    hr = g_device->CreateRasterizerState(&rasterizer_desc, &g_rasterizer_state);
    if (FAILED(hr)) { return -1; }
  }

  // Initialize the viewport. 
	// Doing things this way causes weird glitches when resizing. 
	// It would be better to set the viewport in the where resizing 
	// is handled so that it gets recomputed there instead of initialization
	// which forces the driver to take care of resizing which would definitely
	// cause weird glitches.
  {
    g_viewport.Width = static_cast<float>(client_width);
    g_viewport.Height = static_cast<float>(client_height);
    g_viewport.TopLeftX = 0.0f;
    g_viewport.TopLeftY = 0.0f;
    g_viewport.MinDepth = 0.0f;
    g_viewport.MaxDepth = 1.0f;
  }

  return 0;
}

void init_pipeline() {
	assert(g_device);
	HRESULT hr;

  // Create and init vertex buffer
	{
		D3D11_BUFFER_DESC vertex_buffer_desc = {};
		vertex_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertex_buffer_desc.ByteWidth = sizeof(Vertex) * _countof(g_vertices);
		vertex_buffer_desc.CPUAccessFlags = 0;
		vertex_buffer_desc.Usage = D3D11_USAGE_DEFAULT;

		D3D11_SUBRESOURCE_DATA resource_data = {};
		resource_data.pSysMem = g_vertices;
		HRESULT hr = g_device->CreateBuffer(&vertex_buffer_desc, &resource_data, &g_vertex_buffer);
		if(FAILED(hr)) {
			MessageBox(nullptr, TEXT("Failed to create vertex buffer desc"), TEXT("Fatal Error!"), MB_OK | MB_ICONERROR);
			ExitProcess(1);
		}
	}

	{
		// Initialize the index buffer
		D3D11_SUBRESOURCE_DATA resource_data = {};
		D3D11_BUFFER_DESC index_buffer_desc = {};
		index_buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		index_buffer_desc.ByteWidth = sizeof(WORD) * _countof(g_indices);
		index_buffer_desc.CPUAccessFlags = 0;
		index_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		resource_data.pSysMem = g_indices;
		hr = g_device->CreateBuffer(&index_buffer_desc, &resource_data, &g_index_buffer);
		if(FAILED(hr)) {
			MessageBox(nullptr, TEXT("Failed to create index buffer desc"), TEXT("Fatal Error!"), MB_OK | MB_ICONERROR);
			ExitProcess(1);
		}
	}

	// Initialize the texture sampler
	{
		D3D11_SAMPLER_DESC sampler_desc = {};
		sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		sampler_desc.MipLODBias = 0.0f;
		sampler_desc.MaxAnisotropy = 1;
		sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		sampler_desc.BorderColor[0] = 0;
		sampler_desc.BorderColor[1] = 0;
		sampler_desc.BorderColor[2] = 0;
		sampler_desc.BorderColor[3] = 0;
		sampler_desc.MinLOD = 0;
		sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;

		hr = g_device->CreateSamplerState(&sampler_desc, &g_sampler_state);
		if(FAILED(hr)) {
			MessageBox(nullptr, TEXT("Failed to create constant sampler state"), TEXT("Fatal Error!"), MB_OK | MB_ICONERROR);
			ExitProcess(1);
		}
	}

	// Create texture description
	{
		D3D11_TEXTURE2D_DESC texture_desc = {};
		texture_desc.Height = g_texture_height;
		texture_desc.Width = g_texture_width;
		texture_desc.MipLevels = 0;
		texture_desc.ArraySize = 1;
		texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texture_desc.SampleDesc.Count = 1;
		texture_desc.SampleDesc.Quality = 0;
		texture_desc.Usage = D3D11_USAGE_DEFAULT;
		texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		texture_desc.CPUAccessFlags = 0;
		texture_desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
		
		hr = g_device->CreateTexture2D(&texture_desc, NULL, &g_texture);
		if(FAILED(hr)) {
			MessageBox(nullptr, TEXT("Failed to create texture desc"), TEXT("Fatal Error!"), MB_OK | MB_ICONERROR);
			ExitProcess(1);
		}

		row_pitch = (g_texture_width * 4) * sizeof(u8);
	}

	// Setup shader resource view description
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view = {};
		shader_resource_view.Format =  DXGI_FORMAT_R8G8B8A8_UNORM;
		shader_resource_view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		shader_resource_view.Texture2D.MostDetailedMip = 0;
		shader_resource_view.Texture2D.MipLevels = -1;

		hr = g_device->CreateShaderResourceView(g_texture, &shader_resource_view, &g_shader_rsv);
		if(FAILED(hr)) {
			MessageBox(nullptr, TEXT("Failed to create shader resource view desc"), TEXT("Fatal Error!"), MB_OK | MB_ICONERROR);
			ExitProcess(1);
		}

	}

  // Initialize the content of the constant buffer defined in the vertex shader.
  D3D11_BUFFER_DESC constant_buffer_desc = {};
  constant_buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  constant_buffer_desc.ByteWidth = sizeof(XMMATRIX);
  constant_buffer_desc.CPUAccessFlags = 0;
  constant_buffer_desc.Usage = D3D11_USAGE_DEFAULT;

  for (s8 i = 0; i < _countof(g_constant_buffers); i++) {
    hr = g_device->CreateBuffer(&constant_buffer_desc, nullptr, &g_constant_buffers[i]);
    if(FAILED(hr)) {
			MessageBox(nullptr, TEXT("Failed to create constant buffer desc"), TEXT("Fatal Error!"), MB_OK | MB_ICONERROR);
			ExitProcess(1);
		}
  }

	
}

// Load the compiled shaders
void load_shaders(LPCWSTR vertex_shader_obj, LPCWSTR pixel_shader_obj) {
	ID3DBlob* vertex_shader_blob;
	ID3DBlob* pixel_shader_blob;

	HRESULT hr = D3DReadFileToBlob(vertex_shader_obj, &vertex_shader_blob);
	if(FAILED(hr)) {
		MessageBox(nullptr, TEXT("Failed to read vertex shader blob"), TEXT("Fatal Error!"), MB_OK | MB_ICONERROR);
		ExitProcess(1);
	}

	hr = g_device->CreateVertexShader(vertex_shader_blob->GetBufferPointer(), vertex_shader_blob->GetBufferSize(), 
																		nullptr, &g_vertex_shader);
	if(FAILED(hr)) {
		MessageBox(nullptr, TEXT("Failed to create vertex shader"), TEXT("Fatal Error!"), MB_OK | MB_ICONERROR);
		ExitProcess(1);
	}

  // Create input layout
  D3D11_INPUT_ELEMENT_DESC vertex_layout_desc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 	D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
  };

  hr = g_device->CreateInputLayout(vertex_layout_desc, _countof(vertex_layout_desc), vertex_shader_blob->GetBufferPointer(), 
																	 vertex_shader_blob->GetBufferSize(), &g_input_layout);
  if (FAILED(hr)) { 
    MessageBox(nullptr, TEXT("Failed to create input layout"), TEXT("Fatal Error!"), MB_OK | MB_ICONERROR);
		ExitProcess(1);
	}
  SafeRelease(vertex_shader_blob);

	// Load up pixel shader blob
	hr = D3DReadFileToBlob(pixel_shader_obj, &pixel_shader_blob); 
	if(FAILED(hr)) {
		MessageBox(nullptr, TEXT("Failed to read pixel shader blob"), TEXT("Fatal Error!"), MB_OK | MB_ICONERROR);
		ExitProcess(1);
	}

	// Create pixel shader
	hr = g_device->CreatePixelShader(pixel_shader_blob->GetBufferPointer(), pixel_shader_blob->GetBufferSize(), nullptr, &g_pixel_shader);
	if(FAILED(hr)) {
		MessageBox(nullptr, TEXT("Failed to create vertex shader blob"), TEXT("Fatal Error!"), MB_OK | MB_ICONERROR);
		ExitProcess(1);
	}

	SafeRelease(pixel_shader_blob);

	// Copy the tga imade data into the texture
	g_device_context->UpdateSubresource(g_texture, 0, nullptr, g_texture_data, row_pitch, 0);

	// Generate mipmap levels for attached texture
	g_device_context->GenerateMips(g_shader_rsv);
}

struct TGAHeader {
	u8 data[12];
	u16 width;
	u16 height;
	u8 bpp;
	u8 data2;
};

bool load_tga32bit(char* filename) {
	s32 error, bpp, image_size, index, i, j, k;
	FILE* file_ptr;
	u32 count;
	TGAHeader file_header;
	u8* tga_image;

	error = fopen_s(&file_ptr, filename, "rb");
	if(error != 0) return false;

	count = (u32)fread(&file_header, sizeof(TGAHeader), 1, file_ptr);
	if(count != 1) return false;

	g_texture_width = file_header.width;
	g_texture_height = file_header.height;
	bpp = (s32)file_header.bpp;

	// Check that the bpp is 32 bits and not 24
	if(bpp != 32) return false;

	image_size = g_texture_width * g_texture_height * 4;
	tga_image = new u8[image_size];

	// Read in tga image data
	count = (u32)fread(tga_image, 1, image_size, file_ptr);
	if(count != image_size) return false;

	// Close file
	error = fclose(file_ptr);
	if(error != 0) return false;

	// allocate memory for the tga destination data
	g_texture_data = new u8[image_size];

	// Initialize the index into the tga destination data array
	index = 0;

	// initialize the index into the tga image data
	k = (g_texture_width * g_texture_height * 4) - (g_texture_width * 4);

	// copy TGA image data 
	for(j = 0; j < g_texture_height; j++) {
		for(i = 0; i < g_texture_width; i++) {
			g_texture_data[index + 0] = tga_image[k + 2]; // red
			g_texture_data[index + 1] = tga_image[k + 1]; // green
			g_texture_data[index + 2] = tga_image[k + 0]; // blue
			g_texture_data[index + 3] = tga_image[k + 3]; // alpha

			k += 4;
			index += 4;
		}

		// set the tga image data index back to the prceeding row at the beginning of the columen since its is readin it upside down.
		k -= (g_texture_width * 8);
	}


	tga_image = 0;
	return true;

}

void setup_projection() {
	RECT client_rect;
	GetClientRect(g_window_handle, &client_rect);
	f32 width = (f32)(client_rect.right - client_rect.left);
	f32 height = (f32)(client_rect.bottom - client_rect.top);
	f32 aspect_ratio = width / height;

	XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(90.0f), aspect_ratio, 0.1f, 10.0f);
	g_device_context->UpdateSubresource(g_constant_buffers[ConstantBuffer_Application], 0, nullptr, &projection, 0, 0);
}

void clear_buffer(const f32 colour[4], f32 depth, u8 stencil) {
	g_device_context->ClearRenderTargetView(g_framebuffer_rtv, colour);
	g_device_context->ClearDepthStencilView(g_depth_stencil_view, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depth, stencil);
}

//void Update(float dt) {
//	// --- Camera ---
//	XMVECTOR eye   = XMVectorSet(0, 0, -10, 1);
//	XMVECTOR focus = XMVectorSet(0, 0, 0, 1);
//	XMVECTOR up    = XMVectorSet(0, 1, 0, 0);
//	g_view_matrix = XMMatrixLookAtLH(eye, focus, up);
//	g_device_context->UpdateSubresource(g_constant_buffers[ConstantBuffer_Frame], 0, nullptr, &g_view_matrix, 0, 0);

//	// --- Object world matrix ---
//	static f32 angle = 0.0f;
//	angle += 25.0f * dt;
//	XMVECTOR rotation_axis = XMVectorSet(1, 0, 0, 0);
//	g_world_matrix = XMMatrixRotationAxis(rotation_axis, XMConvertToRadians(angle));
//	//g_world_matrix = XMMatrixIdentity();
//	g_device_context->UpdateSubresource(g_constant_buffers[ConstantBuffer_Object], 0, nullptr, &g_world_matrix, 0, 0);
//}

void Update(float dt) {
	// --- Camera ---
	XMVECTOR eye   = XMVectorSet(0, 0, 2.6, 1); // move the camera back a bit
	XMVECTOR focus = XMVectorSet(0, 0, 0, 1);
	XMVECTOR up    = XMVectorSet(0, 1, 1, 0);
	g_view_matrix = XMMatrixLookAtLH(eye, focus, up);
	g_device_context->UpdateSubresource(g_constant_buffers[ConstantBuffer_Frame], 0, nullptr, &g_view_matrix, 0, 0);

	// --- Object world matrix ---
	static float angle = 0.0f;
	angle += 90.0f * dt; // 90 degrees per second
	if (angle > 360.0f) angle -= 360.0f; // wrap around for numerical stability

	// Rotate around Z-axis (spins in XY plane facing camera)
	//g_world_matrix = XMMatrixRotationZ(XMConvertToRadians(angle));
	g_world_matrix = XMMatrixIdentity();
	g_world_matrix = XMMatrixRotationAxis(XMVectorSet(0, 1, 0, 0), XMConvertToRadians(angle));

	// Optional: translate to keep triangle centered if needed
	//XMMATRIX translation = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
	//g_world_matrix = g_world_matrix * translation;

	g_device_context->UpdateSubresource(g_constant_buffers[ConstantBuffer_Object], 0, nullptr, &g_world_matrix, 0, 0);
}


void Render() {
	// Resizes are left to the driver to handle currently, for smoother transitions,
	// it is best to handle this manually.
	assert(g_device);
	assert(g_device_context);

	f32 black[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	clear_buffer(black, 1.0f, 0);

	// setup input assembler
  const UINT vertex_stride = sizeof(Vertex);
  const UINT offset = 0;

  g_device_context->IASetVertexBuffers(0, 1, &g_vertex_buffer, &vertex_stride, &offset);
  g_device_context->IASetInputLayout(g_input_layout);
  g_device_context->IASetIndexBuffer(g_index_buffer, DXGI_FORMAT_R16_UINT, 0);
  g_device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // setup the vertex shader stage
  g_device_context->VSSetShader(g_vertex_shader, nullptr, 0);
  g_device_context->VSSetConstantBuffers(0, 3, g_constant_buffers);

	// Set the shader texture resource in the pixel shader
	g_device_context->PSSetShaderResources(0, 1, &g_shader_rsv);

	// Setup the sample state
	g_device_context->PSSetSamplers(0, 1, &g_sampler_state);

  // setup rasterizer stage
  g_device_context->RSSetState(g_rasterizer_state);
  g_device_context->RSSetViewports(1, &g_viewport);

  // setup pixel shader
  g_device_context->PSSetShader(g_pixel_shader, nullptr, 0);

  // setup output merger stage
  g_device_context->OMSetRenderTargets(1, &g_framebuffer_rtv, g_depth_stencil_view);
  g_device_context->OMSetDepthStencilState(g_depth_stencil_state, 1);

  g_device_context->DrawIndexed(_countof(g_indices), 0, 0);

	if (g_enable_vsync) {
    g_swapchain->Present(1, 0);
  } else {
    g_swapchain->Present(0, 0);
  }
}

int Run() {
  MSG msg = {0};

  local DWORD prev_time = timeGetTime();
  local const float target_framerate = 30.0f;
  local const float max_time_step = 1.0f / target_framerate;

  while (msg.message != WM_QUIT) {
    if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      DWORD current_time = timeGetTime();
      float dt = (current_time - prev_time) / 1000.0f;
      prev_time = current_time;

      // cap delta time to the max time step
      dt = std::min<float>(dt, max_time_step);
      Update(dt);
      Render();

    }
  }
  return static_cast<int>(msg.wParam);
}

void unload_pipeline() {
	SafeRelease(g_sampler_state);
  SafeRelease(g_constant_buffers[ConstantBuffer_Application]);
  SafeRelease(g_constant_buffers[ConstantBuffer_Frame]);
  SafeRelease(g_constant_buffers[ConstantBuffer_Object]);
  SafeRelease(g_index_buffer);
  SafeRelease(g_vertex_buffer);
  SafeRelease(g_input_layout);
  SafeRelease(g_vertex_shader);
  SafeRelease(g_pixel_shader);
}

void shutdown_directx() {
  SafeRelease(g_depth_stencil_view);
  SafeRelease(g_framebuffer_rtv);
  SafeRelease(g_depth_stencil_buffer);
  SafeRelease(g_depth_stencil_state);
  SafeRelease(g_rasterizer_state);
  SafeRelease(g_swapchain);
  SafeRelease(g_device_context);
  SafeRelease(g_device);
}

void system_cleanup() {
	// Release teh tga image data once copied to the destination array
	delete[] g_texture_data;
}


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE prevInstance, LPWSTR cmdLine, int cmdShow) {
  UNREFERENCED_PARAMETER(prevInstance);
  UNREFERENCED_PARAMETER(cmdLine);

  // Check for DirectX Math library support.
  if (!XMVerifyCPUSupport()) {
    MessageBox(nullptr, TEXT("Failed to verify DirectX Math library support."), TEXT("Error"), MB_OK);
    return -1;
  }

  if (init_application(hInstance, cmdShow) != 0) {
    MessageBox(nullptr, TEXT("Failed to create applicaiton window."), TEXT("Error"), MB_OK);
    return -1;
  }

	load_tga32bit("data/stone01.tga");

  if (init_directx(hInstance, g_enable_vsync) != 0) {
    MessageBox(nullptr, TEXT("Error occured while initializing the GPU"), TEXT("Fatal Error"), MB_OK);
    return -1;
  }
	
	init_pipeline();

	load_shaders(L"data/shaders/textured_vs.cso", L"data/shaders/textured_ps.cso");

	setup_projection();

  int code = Run();

  unload_pipeline();
  shutdown_directx();
	system_cleanup();

  return code;
}