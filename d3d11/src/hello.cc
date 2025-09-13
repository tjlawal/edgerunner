#include <windows.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>

#include <iostream>
#include <string>

// Static libs
#pragma comment(lib, "d3d11")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "d3dcompiler")
#pragma comment(lib, "user32")
#pragma comment(lib, "Winmm")

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
const LONG g_window_width 				 = 1280;
const LONG g_window_height 				 = 720;
LPCSTR g_window_class_name = "[edgerunner-gfx-class]";
LPCSTR g_window_name = "Edgerunner";
HWND g_window_handle = 0;
const BOOL g_enable_vsync = false;

// Direct3D device and swapchain
ID3D11Device *g_device = nullptr;
ID3D11DeviceContext *g_device_context = nullptr;
IDXGISwapChain *g_swapchain = nullptr;
ID3D11RenderTargetView *g_framebuffer_rtv = nullptr;			// Render target view for the back buffer in the swapchain

ID3D11DepthStencilView *g_depth_stencil_view = nullptr;   // Depth/stencil view for use as a depth buffer
ID3D11Texture2D *g_depth_stencil_buffer = nullptr;        // Texture to associate to the depth stencil view
ID3D11DepthStencilState *g_depth_stencil_state = nullptr; // Functionality for the depth/stencil stages

ID3D11RasterizerState *g_rasterizer_state = nullptr;		  // Functionality for the rasterizer stage.
D3D11_VIEWPORT g_viewport = {};

// Vertex buffer data
ID3D11InputLayout *g_input_layout = nullptr;
ID3D11Buffer *g_vertex_buffer = nullptr;
ID3D11Buffer *g_index_buffer = nullptr;

// Shader data
ID3D11VertexShader *g_vertex_shader = nullptr;
ID3D11PixelShader *g_pixel_shader = nullptr;

// [ConstantBuffer_Application] Buffers used to update the constant variables declared in the vertex shader
// Stores data in the shader that rarely changes such as the projection camera projection
// matrix which is set when the render window is created and only changed on resize.

// [ConstantBuffer_Frame] Stores data that changes every frame such as the camera view matrix which changes every time
// the camera moves.

// [ConstantBuffer_Obj] Stores the different data for overy object being rendered. Example includes an object's
// world matrix. Since each object in the scene will have a different world matrix, this shader
// variable would be updated for every seperate draw call
enum ConstantBuffer : u64 {
  ConstantBuffer_Application,
  ConstantBuffer_Frame,
  ConstantBuffer_Object,
  ConstantBuffer_COUNT
};

ID3D11Buffer *g_constant_buffers[ConstantBuffer_COUNT];

XMMATRIX g_world_matrix;      // Stores every single object world matrix being rendererd.
XMMATRIX g_view_matrix;       // Stores the camera view matrix, updated onces every frame
XMMATRIX g_projection_matrix; // Stores the projection matrix, updated at window creation

// Vertex data for a colored cube.
struct VertexPosColour {
  XMFLOAT3 position;
  XMFLOAT3 colour;
};

VertexPosColour g_vertices[8] = {
    {XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f)}, // 0
    {XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)},  // 1
    {XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(1.0f, 1.0f, 0.0f)},   // 2
    {XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f)},  // 3
    {XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f)},  // 4
    {XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 1.0f)},   // 5
    {XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(1.0f, 1.0f, 1.0f)},    // 6
    {XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 1.0f)}    // 7
};

WORD g_indicies[36] = {
	0, 1, 2, 0, 2, 3, 
	4, 6, 5, 4, 7, 6, 
	4, 5, 1, 4, 1, 0, 
	3, 2, 6, 3, 6, 7, 
	1, 5, 6, 1, 6, 2, 
	4, 0, 3, 4, 3, 7
};

// Forward declarations.
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

void Update(f32 deltaTime);
void Render();
void Cleanup();

/**
 * Initialize the application window.
 */
int InitApplication(HINSTANCE hInstance, int cmdShow) {
  WNDCLASSEX wndClass = {0};
  wndClass.cbSize = sizeof(WNDCLASSEX);
  wndClass.style = CS_HREDRAW | CS_VREDRAW;
  wndClass.lpfnWndProc = &WndProc;
  wndClass.hInstance = hInstance;
  wndClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wndClass.lpszMenuName = nullptr;
  wndClass.lpszClassName = g_window_class_name;

  if (!RegisterClassEx(&wndClass)) {
    return -1;
  }

  RECT windowRect = {0, 0, g_window_width, g_window_height};
  AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

  g_window_handle = CreateWindowA(g_window_class_name, g_window_name, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                  windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, nullptr,
                                  nullptr, hInstance, nullptr);

  if (!g_window_handle) {
    return -1;
  }

  ShowWindow(g_window_handle, cmdShow);
  UpdateWindow(g_window_handle);

  return 0;
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
  default:
    return DefWindowProc(hwnd, message, wParam, lParam);
  }

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

    hr = g_device->CreateTexture2D(&depth_stencil_buffer_desc, nullptr, 
																	 &g_depth_stencil_buffer);
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
    rasterizer_desc.CullMode = D3D11_CULL_BACK;
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
	// Although not part of DirectX initialization phase, it can be defered to say
  // `gpu_end_frame` function that actually submits stuff to the GPU for rendering. 
	// Here we just do it at initialization and forget it.
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

bool load_content() {
  assert(g_device);

  // Create and init vertex buffer
  D3D11_BUFFER_DESC vertex_buffer_desc = {};
  vertex_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  vertex_buffer_desc.ByteWidth = sizeof(VertexPosColour) * _countof(g_vertices);
  vertex_buffer_desc.CPUAccessFlags = 0;
  vertex_buffer_desc.Usage = D3D11_USAGE_DEFAULT;

  D3D11_SUBRESOURCE_DATA resource_data = {};
  resource_data.pSysMem = g_vertices;
  HRESULT hr = g_device->CreateBuffer(&vertex_buffer_desc, &resource_data, &g_vertex_buffer);
  if (FAILED(hr)) { return false; }

  // Initialize the index buffer
  D3D11_BUFFER_DESC index_buffer_desc = {};
  index_buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
  index_buffer_desc.ByteWidth = sizeof(WORD) * _countof(g_indicies);
  index_buffer_desc.CPUAccessFlags = 0;
  index_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
  resource_data.pSysMem = g_indicies;
  hr = g_device->CreateBuffer(&index_buffer_desc, &resource_data, &g_index_buffer);
  if (FAILED(hr)) { return false; }

  // Initialize the content of the constant buffer defined in the vertex shader.
  D3D11_BUFFER_DESC constant_buffer_desc = {};
  constant_buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  constant_buffer_desc.ByteWidth = sizeof(XMMATRIX);
  constant_buffer_desc.CPUAccessFlags = 0;
  constant_buffer_desc.Usage = D3D11_USAGE_DEFAULT;

  for (s8 i = 0; i < _countof(g_constant_buffers); i++) {
    hr = g_device->CreateBuffer(&constant_buffer_desc, nullptr, &g_constant_buffers[i]);
    if (FAILED(hr)) {
      return false;
    }
  }

	// Load the compiled shaders
	ID3DBlob* vertex_shader_blob;
	ID3DBlob* pixel_shader_blob;

	LPCWSTR	vertex_shader_obj = L"data/shaders/default_vs.cso";
	LPCWSTR	pixel_shader_obj = L"data/shaders/default_ps.cso";

	hr = D3DReadFileToBlob(vertex_shader_obj, &vertex_shader_blob);
	if(FAILED(hr)) { return false; }

	hr = g_device->CreateVertexShader(vertex_shader_blob->GetBufferPointer(), 
																		vertex_shader_blob->GetBufferSize(), nullptr, &g_vertex_shader);
	if(FAILED(hr)) return false;

  // Create input layout
  D3D11_INPUT_ELEMENT_DESC vertex_layout_desc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  offsetof(VertexPosColour, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOUR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(VertexPosColour, colour), D3D11_INPUT_PER_VERTEX_DATA, 0 },
  };

  hr = g_device->CreateInputLayout(vertex_layout_desc, _countof(vertex_layout_desc), vertex_shader_blob->GetBufferPointer(), vertex_shader_blob->GetBufferSize(), &g_input_layout);
  if (FAILED(hr)) { return false; }
  SafeRelease(vertex_shader_blob);

	hr = D3DReadFileToBlob(pixel_shader_obj, &pixel_shader_blob); 
  if (FAILED(hr)) { return false; }

	hr = g_device->CreatePixelShader(pixel_shader_blob->GetBufferPointer(), pixel_shader_blob->GetBufferSize(), nullptr, &g_pixel_shader);
	if(FAILED(hr)) return false;

	SafeRelease(pixel_shader_blob);

  // Setup projection matrix
  RECT client_rect;
  GetClientRect(g_window_handle, &client_rect);
  float client_width = static_cast<float>(client_rect.right - client_rect.left);
  float client_height = static_cast<float>(client_rect.bottom - client_rect.top);
	float aspect_ratio = client_width / client_height;

	g_projection_matrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(90.0f), aspect_ratio, 0.1f, 1000.0f);
	g_device_context->UpdateSubresource(g_constant_buffers[ConstantBuffer_Application], 0, nullptr, &g_projection_matrix, 0, 0);

  return true;
}

void Update(float dt) {
	// --- Camera ---
	XMVECTOR eye   = XMVectorSet(0, 0, -10, 1);
	XMVECTOR focus = XMVectorSet(0, 0, 0, 1);
	XMVECTOR up    = XMVectorSet(0, 1, 0, 0);
	g_view_matrix = XMMatrixLookAtLH(eye, focus, up);
	g_device_context->UpdateSubresource(g_constant_buffers[ConstantBuffer_Frame], 0, nullptr, &g_view_matrix, 0, 0);

	// --- Object world matrix ---
	static f32 angle = 0.0f;
	angle += 90.0f * dt;
	XMVECTOR rotation_axis = XMVectorSet(2, 1, 0, 0);
	g_world_matrix = XMMatrixRotationAxis(rotation_axis, XMConvertToRadians(angle));
	//g_world_matrix = XMMatrixIdentity();
	g_device_context->UpdateSubresource(g_constant_buffers[ConstantBuffer_Object], 0, nullptr, &g_world_matrix, 0, 0);
}

// Clear the color and depth buffers.
void Clear(const FLOAT clearColor[4], FLOAT clearDepth, UINT8 clearStencil) {
  g_device_context->ClearRenderTargetView(g_framebuffer_rtv, clearColor);
  g_device_context->ClearDepthStencilView(g_depth_stencil_view, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, clearDepth, clearStencil);
}

void Present(bool vSync) {
  if (vSync) {
    g_swapchain->Present(1, 0);
  } else {
    g_swapchain->Present(0, 0);
  }
}

void Render() {
  assert(g_device);
  assert(g_device_context);

  FLOAT CornflowerBlue[4] = {0.0f, 0.0f, 0.0f, 0.0f};

  Clear(CornflowerBlue, 1.0f, 0);

  // setup input assembler
  const UINT vertex_stride = sizeof(VertexPosColour);
  const UINT offset = 0;

  g_device_context->IASetVertexBuffers(0, 1, &g_vertex_buffer, &vertex_stride, &offset);
  g_device_context->IASetInputLayout(g_input_layout);
  g_device_context->IASetIndexBuffer(g_index_buffer, DXGI_FORMAT_R16_UINT, 0);
  g_device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // setup the vertex shader stage
  g_device_context->VSSetShader(g_vertex_shader, nullptr, 0);
  g_device_context->VSSetConstantBuffers(0, 3, g_constant_buffers);

  // setup rasterizer stage
  g_device_context->RSSetState(g_rasterizer_state);
  g_device_context->RSSetViewports(1, &g_viewport);

  // setup pixel shader
  g_device_context->PSSetShader(g_pixel_shader, nullptr, 0);

  // setup output merger stage
  g_device_context->OMSetRenderTargets(1, &g_framebuffer_rtv, g_depth_stencil_view);
  g_device_context->OMSetDepthStencilState(g_depth_stencil_state, 1);

  g_device_context->DrawIndexed(_countof(g_indicies), 0, 0);

  Present(g_enable_vsync);
}

void UnloadContent() {
  SafeRelease(g_constant_buffers[ConstantBuffer_Application]);
  SafeRelease(g_constant_buffers[ConstantBuffer_Frame]);
  SafeRelease(g_constant_buffers[ConstantBuffer_Object]);
  SafeRelease(g_index_buffer);
  SafeRelease(g_vertex_buffer);
  SafeRelease(g_input_layout);
  SafeRelease(g_vertex_shader);
  SafeRelease(g_pixel_shader);
}

void Cleanup() {
  SafeRelease(g_depth_stencil_view);
  SafeRelease(g_framebuffer_rtv);
  SafeRelease(g_depth_stencil_buffer);
  SafeRelease(g_depth_stencil_state);
  SafeRelease(g_rasterizer_state);
  SafeRelease(g_swapchain);
  SafeRelease(g_device_context);
  SafeRelease(g_device);
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

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE prevInstance, LPWSTR cmdLine, int cmdShow) {
  UNREFERENCED_PARAMETER(prevInstance);
  UNREFERENCED_PARAMETER(cmdLine);

  // Check for DirectX Math library support.
  if (!XMVerifyCPUSupport()) {
    MessageBox(nullptr, TEXT("Failed to verify DirectX Math library support."), TEXT("Error"), MB_OK);
    return -1;
  }

  if (InitApplication(hInstance, cmdShow) != 0) {
    MessageBox(nullptr, TEXT("Failed to create applicaiton window."), TEXT("Error"), MB_OK);
    return -1;
  }

  if (init_directx(hInstance, g_enable_vsync) != 0) {
    MessageBox(nullptr, TEXT("Error occured while initializing the GPU"), TEXT("Fatal Error"), MB_OK);
    return -1;
  }

  if (!load_content()) {
    MessageBox(nullptr, TEXT("Failed to load content."), TEXT("Fatal Error"), MB_OK);
    return -1;
  }

  int returnCode = Run();

  UnloadContent();
  Cleanup();

  return returnCode;
}