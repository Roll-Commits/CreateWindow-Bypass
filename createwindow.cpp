#pragma once

#include <Windows.h>
#include <uxtheme.h>
#include <dwmapi.h>

#include <d3d11.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"

#include "render.h"

#pragma comment(lib, "d3d11.lib")

#include <utility>
#include <random>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static ID3D11Device* m_d3d_device;
static ID3D11DeviceContext* m_d3d_context;
static IDXGISwapChain* m_d3d_swapchain;
static ID3D11RenderTargetView* m_render_target;

int GetRandomInteger()
{

	std::random_device RANDOM;
	std::mt19937_64 RANDOM_ENGINE(RANDOM());

	std::uniform_int_distribution<int> DISTRIBUTION(5, 13);

	return DISTRIBUTION(RANDOM_ENGINE);

}

std::string GetRandomString(size_t length)
{

	const std::string ALPHA_BET = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

	std::random_device RANDOM;
	std::default_random_engine RANDOM_ENGINE(RANDOM());
	std::uniform_int_distribution<size_t> DISTRIBUTION(0, ALPHA_BET.size() - 1);

	std::string output;

	while (output.size() < length)
		output += ALPHA_BET[DISTRIBUTION(RANDOM_ENGINE)];

	return output;

}

enum ZBID
{
	ZBID_DEFAULT = 0,
	ZBID_DESKTOP = 1,
	ZBID_UIACCESS = 2,
	ZBID_IMMERSIVE_IHM = 3,
	ZBID_IMMERSIVE_NOTIFICATION = 4,
	ZBID_IMMERSIVE_APPCHROME = 5,
	ZBID_IMMERSIVE_MOGO = 6,
	ZBID_IMMERSIVE_EDGY = 7,
	ZBID_IMMERSIVE_INACTIVEMOBODY = 8,
	ZBID_IMMERSIVE_INACTIVEDOCK = 9,
	ZBID_IMMERSIVE_ACTIVEMOBODY = 10,
	ZBID_IMMERSIVE_ACTIVEDOCK = 11,
	ZBID_IMMERSIVE_BACKGROUND = 12,
	ZBID_IMMERSIVE_SEARCH = 13,
	ZBID_GENUINE_WINDOWS = 14,
	ZBID_IMMERSIVE_RESTRICTED = 15,
	ZBID_SYSTEM_TOOLS = 16,
	ZBID_LOCK = 17,
	ZBID_ABOVELOCK_UX = 18,
};

class c_overlay
{

private:

	static LRESULT __stdcall window_procedure(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
	{

		if (ImGui_ImplWin32_WndProcHandler(hwnd, message, w_param, l_param))
			return true;

		switch (message)
		{

		case WM_SIZE:

			if (m_d3d_device != NULL && w_param != SIZE_MINIMIZED)
			{

				if (m_render_target) {
					m_render_target->Release(); m_render_target = NULL;
				}

				m_d3d_swapchain->ResizeBuffers(0, (UINT)LOWORD(l_param), (UINT)HIWORD(l_param), DXGI_FORMAT_UNKNOWN, 0);

				ID3D11Texture2D* pBackBuffer;
				m_d3d_swapchain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
				m_d3d_device->CreateRenderTargetView(pBackBuffer, NULL, &m_render_target);
				pBackBuffer->Release();

			}
			return 0;

		case WM_SYSCOMMAND:

			if ((w_param & 0xfff0) == SC_KEYMENU)
				return 0;

			break;

		case WM_DESTROY:

			PostQuitMessage(0);
			return 0;

		}

		return DefWindowProc(hwnd, message, w_param, l_param);

	}

	HWND m_overlay_window{};
	HWND m_game_window{};

	const char* m_window_name;
	const char* m_window_class_name;

	WNDCLASSEX m_window_class{};

	ImVec2 m_window_size{};

	RECT m_old_rect{};
	MSG m_window_message{};

	bool m_is_input_allowed{};

public:

	bool initialize(const char* window_name, const char* class_name = NULL)
	{

		m_game_window = FindWindowA(class_name, window_name); 
		
		if (m_game_window == INVALID_HANDLE_VALUE) 
			return false;

		m_window_name = window_name;
		m_window_class_name = class_name;

		auto overlay_class_name = GetRandomString(GetRandomInteger());
		auto overlay_window_name = GetRandomString(GetRandomInteger());

		m_window_class = {
			sizeof(WNDCLASSEX),
			0,
			&c_overlay::window_procedure,
			0,
			0,
			nullptr,
			NULL,
			NULL,
			0,
			0,
			overlay_class_name.c_str(),
			LoadIcon(0, IDI_APPLICATION)
		};

		ATOM window_atom = RegisterClassExA(&m_window_class);

		if (!window_atom)
			return false;

		RECT game_rect{};

		if (!GetClientRect(m_game_window, &game_rect))
			return false;

		m_window_size = ImVec2(game_rect.right - game_rect.left, game_rect.bottom - game_rect.top);

		m_overlay_window = CreateWindowExA(NULL, overlay_class_name.c_str(), overlay_window_name.c_str(), WS_POPUP | WS_VISIBLE, static_cast<int>(game_rect.left - 3.f), static_cast<int>(game_rect.top - 3.f), static_cast<int>(m_window_size.x + 3.f), static_cast<int>(m_window_size.y + 3.f), NULL, NULL, NULL, NULL);

		if (m_overlay_window == INVALID_HANDLE_VALUE)
			return false;

		static MARGINS window_margins = { static_cast<int>(game_rect.left - 3.f), static_cast<int>(game_rect.top - 3.f), static_cast<int>(m_window_size.x + 3.f), static_cast<int>(m_window_size.y + 3.f) };

		if (FAILED(DwmExtendFrameIntoClientArea(m_overlay_window, &window_margins)))
			return false;

		ShowWindow(m_overlay_window, SW_SHOW);

		if (!UpdateWindow(m_overlay_window))
			return false;

		SetWindowLongA(m_overlay_window, GWL_EXSTYLE, WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW);

		bool create_device = false;

		DXGI_SWAP_CHAIN_DESC sd;

		{

			ZeroMemory(&sd, sizeof(sd));
			sd.BufferCount = 1;
			sd.BufferDesc.Width = 0;
			sd.BufferDesc.Height = 0;
			sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
			sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			sd.OutputWindow = m_overlay_window;
			sd.SampleDesc.Count = 1;
			sd.SampleDesc.Quality = 0;
			sd.Windowed = TRUE;
			sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		}

		UINT create_device_flags = 0;
		D3D_FEATURE_LEVEL feature_level;
		const D3D_FEATURE_LEVEL feature_level_array[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
		if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, create_device_flags, feature_level_array, 2, D3D11_SDK_VERSION, &sd, &m_d3d_swapchain, &m_d3d_device, &feature_level, &m_d3d_context) != S_OK)
			create_device = false;
		else
			create_device = true;

		ID3D11Texture2D* pBackBuffer{ NULL };
		m_d3d_swapchain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
		m_d3d_device->CreateRenderTargetView(pBackBuffer, NULL, &m_render_target);
		pBackBuffer->Release();

		if (!create_device)
		{

			if (m_render_target)
			{
				m_render_target->Release();
				m_render_target = NULL;
			}

			if (m_d3d_swapchain)
			{
				m_d3d_swapchain->Release();
				m_d3d_swapchain = NULL;
			}

			if (m_d3d_context)
			{
				m_d3d_context->Release();
				m_d3d_context = NULL;
			}

			if (m_d3d_device)
			{
				m_d3d_device->Release();
				m_d3d_device = NULL;
			}

			UnregisterClass(m_window_class.lpszClassName, m_window_class.hInstance);

			return false;

		}
		
		ImGui::CreateContext();

		ImGui::GetIO().IniFilename = NULL;

		ImGui_ImplWin32_Init(m_overlay_window);
		ImGui_ImplDX11_Init(m_d3d_device, m_d3d_context);

		return true;

	}

	void destroy()
	{

		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		if (m_render_target)
		{
			m_render_target->Release();
			m_render_target = NULL;
		}

		if (m_d3d_swapchain)
		{
			m_d3d_swapchain->Release();
			m_d3d_swapchain = NULL;
		}

		if (m_d3d_context)
		{
			m_d3d_context->Release();
			m_d3d_context = NULL;
		}

		if (m_d3d_device)
		{
			m_d3d_device->Release();
			m_d3d_device = NULL;
		}

		DestroyWindow(m_overlay_window);
		UnregisterClass(m_window_class.lpszClassName, m_window_class.hInstance);
		
		exit(0);

	}

	bool is_invalid_window() const { return (FindWindowA(m_window_class_name, m_window_name) == NULL); }

	void start_render()
	{

		if (is_invalid_window())
			destroy();

		if (PeekMessage(&m_window_message, m_overlay_window, NULL, NULL, PM_REMOVE))
		{

			TranslateMessage(&m_window_message);
			DispatchMessage(&m_window_message);

		}

		auto foreground_window = GetForegroundWindow();

		if (m_game_window == foreground_window)
		{

			auto previous_window = GetWindow(foreground_window, GW_HWNDPREV);
			SetWindowPos(m_overlay_window, previous_window, NULL, NULL, NULL, NULL, SWP_NOMOVE | SWP_NOSIZE);

		}

		RECT rect{};
		POINT cord{};

		GetClientRect(m_game_window, &rect);

		ClientToScreen(m_game_window, &cord);

		rect.left = cord.x;
		rect.top = cord.y;

		if (rect != m_old_rect)
		{

			m_old_rect = rect;
			m_window_size = ImVec2(rect.right, rect.bottom);

			SetWindowPos(m_overlay_window, NULL, cord.x, cord.y, static_cast<int>(rect.right + 3.f), static_cast<int>(rect.bottom + 3.f), SWP_NOREDRAW);

			ImGui_ImplDX11_InvalidateDeviceObjects();
			ImGui_ImplDX11_CreateDeviceObjects();

		}

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

	}

	void present_render(bool vsync)
	{

		ImGui::Render();

		static const float clear_color_with_alpha[4] = { 0,0,0,0 };

		m_d3d_context->OMSetRenderTargets(1, &m_render_target, NULL);
		m_d3d_context->ClearRenderTargetView(m_render_target, clear_color_with_alpha);

		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		m_d3d_swapchain->Present((int)vsync, 0);

	}

	void change_input_allowed(bool allowed) 
	{

		auto overlay_style = GetWindowLong(m_overlay_window, GWL_EXSTYLE);

		if (allowed && !m_is_input_allowed)
		{

			overlay_style &= ~WS_EX_LAYERED;
			
			SetWindowLong(m_overlay_window, GWL_EXSTYLE, overlay_style);
			SetFocus(m_overlay_window);

			m_is_input_allowed = true;

		}
		else if (!allowed && m_is_input_allowed)
		{

			overlay_style |= WS_EX_LAYERED;

			SetWindowLong(m_overlay_window, GWL_EXSTYLE, overlay_style);
			SetFocus(m_overlay_window);

			m_is_input_allowed = false;

		}

	}

	HWND get_overlay_window() const { return m_overlay_window; }
	HWND get_game_window() const { return m_game_window; }

	auto get_message() const { return m_window_message.message; }

	ImVec2 get_window_size() const { return m_window_size; }

}; inline c_overlay overlay{};
