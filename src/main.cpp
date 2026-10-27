#include <windows.h>

#include <d3d11.h>
#include <shellapi.h>
#include <wrl/client.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>

#include "application.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
namespace {
using Microsoft::WRL::ComPtr;
ComPtr<ID3D11Device> g_device;
ComPtr<ID3D11DeviceContext> g_context;
ComPtr<IDXGISwapChain> g_swap;
ComPtr<ID3D11RenderTargetView> g_target;
drawer::Application* g_application = nullptr;
bool g_minimized = false;
void Check(HRESULT hr, const char* operation) {
    if (FAILED(hr))
        throw std::runtime_error(std::string(operation) + " failed: " + std::to_string(hr));
}
void Target() {
    ComPtr<ID3D11Texture2D> texture;
    Check(g_swap->GetBuffer(0, IID_PPV_ARGS(&texture)), "GetBuffer");
    Check(g_device->CreateRenderTargetView(texture.Get(), nullptr, &g_target),
          "CreateRenderTargetView");
}
LRESULT WINAPI WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (ImGui_ImplWin32_WndProcHandler(window, message, wparam, lparam)) return true;
    switch (message) {
        case WM_SIZE:
            g_minimized = wparam == SIZE_MINIMIZED;
            if (g_device && !g_minimized) {
                g_context->OMSetRenderTargets(0, nullptr, nullptr);
                g_target.Reset();
                if (SUCCEEDED(g_swap->ResizeBuffers(0, LOWORD(lparam), HIWORD(lparam),
                                                    DXGI_FORMAT_UNKNOWN, 0))) {
                    try {
                        Target();
                    } catch (...) {
                        PostQuitMessage(1);
                    }
                }
            }
            return 0;
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
            info->ptMinTrackSize = {1000, 650};
            return 0;
        }
        case WM_DPICHANGED: {
            auto* bounds = reinterpret_cast<RECT*>(lparam);
            SetWindowPos(window, nullptr, bounds->left, bounds->top, bounds->right - bounds->left,
                         bounds->bottom - bounds->top, SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_SYSCOMMAND:
            if ((wparam & 0xfff0) == SC_KEYMENU) return 0;
            break;
        case WM_CLOSE:
            if (g_application)
                g_application->RequestExit();
            else
                DestroyWindow(window);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
void Capture(const std::filesystem::path& path) {
    ComPtr<ID3D11Texture2D> texture, staging;
    Check(g_swap->GetBuffer(0, IID_PPV_ARGS(&texture)), "Screenshot buffer");
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    Check(g_device->CreateTexture2D(&desc, nullptr, &staging), "Screenshot texture");
    g_context->CopyResource(staging.Get(), texture.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    Check(g_context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped), "Screenshot map");
    BITMAPFILEHEADER file{};
    BITMAPINFOHEADER info{};
    info.biSize = sizeof(info);
    info.biWidth = desc.Width;
    info.biHeight = -static_cast<LONG>(desc.Height);
    info.biPlanes = 1;
    info.biBitCount = 32;
    info.biCompression = BI_RGB;
    info.biSizeImage = desc.Width * desc.Height * 4;
    file.bfType = 0x4d42;
    file.bfOffBits = sizeof(file) + sizeof(info);
    file.bfSize = file.bfOffBits + info.biSizeImage;
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(&file), sizeof(file));
    output.write(reinterpret_cast<const char*>(&info), sizeof(info));
    for (UINT y = 0; y < desc.Height; ++y) {
        const auto* row = static_cast<const unsigned char*>(mapped.pData) + y * mapped.RowPitch;
        for (UINT x = 0; x < desc.Width; ++x) {
            const unsigned char bgra[] = {row[x * 4 + 2], row[x * 4 + 1], row[x * 4], 255};
            output.write(reinterpret_cast<const char*>(bgra), 4);
        }
    }
    g_context->Unmap(staging.Get(), 0);
}
}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    HWND window = nullptr;
    bool win32_ready = false, dx_ready = false;
    int result = 0;
    try {
        int argc = 0;
        auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        bool smoke = false;
        std::filesystem::path screenshot;
        for (int i = 1; i < argc; ++i) {
            if (std::wstring(argv[i]) == L"--smoke-test") smoke = true;
            if (std::wstring(argv[i]) == L"--screenshot" && i + 1 < argc) screenshot = argv[++i];
        }
        LocalFree(argv);
        ImGui_ImplWin32_EnableDpiAwareness();
        WNDCLASSEXW type{sizeof(type),
                         CS_CLASSDC,
                         WindowProc,
                         0,
                         0,
                         instance,
                         nullptr,
                         LoadCursorW(nullptr, IDC_ARROW),
                         nullptr,
                         nullptr,
                         L"DrawerWindow",
                         nullptr};
        if (!RegisterClassExW(&type)) throw std::runtime_error("RegisterClass failed");
        window = CreateWindowW(type.lpszClassName, L"Drawer — 二维交互测试工具",
                               WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1440, 900,
                               nullptr, nullptr, instance, nullptr);
        if (!window) throw std::runtime_error("CreateWindow failed");
        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferCount = 2;
        desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.OutputWindow = window;
        desc.SampleDesc.Count = 1;
        desc.Windowed = TRUE;
        desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
        auto hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                                levels, 2, D3D11_SDK_VERSION, &desc, &g_swap,
                                                &g_device, nullptr, &g_context);
        if (FAILED(hr))
            hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2,
                                               D3D11_SDK_VERSION, &desc, &g_swap, &g_device,
                                               nullptr, &g_context);
        Check(hr, "D3D11 device");
        Target();
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        const float dpi = ImGui_ImplWin32_GetDpiScaleForHwnd(window);
        wchar_t windows_dir[MAX_PATH]{};
        GetWindowsDirectoryW(windows_dir, MAX_PATH);
        auto font = std::filesystem::path(windows_dir) / L"Fonts/msyh.ttc";
        if (std::filesystem::exists(font))
            io.Fonts->AddFontFromFileTTF(font.u8string().c_str(), 17 * dpi, nullptr,
                                         io.Fonts->GetGlyphRangesChineseFull());
        ImGui::StyleColorsDark();
        auto& style = ImGui::GetStyle();
        style.WindowRounding = 0;
        style.ChildRounding = 6;
        style.FrameRounding = 4;
        style.FramePadding = {8, 6};
        style.ItemSpacing = {8, 8};
        style.Colors[ImGuiCol_WindowBg] = {.07f, .09f, .13f, 1};
        style.Colors[ImGuiCol_Button] = {.13f, .23f, .35f, 1};
        style.Colors[ImGuiCol_ButtonHovered] = {.18f, .36f, .52f, 1};
        style.ScaleAllSizes(dpi);
        win32_ready = ImGui_ImplWin32_Init(window);
        dx_ready = ImGui_ImplDX11_Init(g_device.Get(), g_context.Get());
        if (!win32_ready || !dx_ready)
            throw std::runtime_error("ImGui backend initialization failed");
        wchar_t executable[32768]{};
        GetModuleFileNameW(nullptr, executable, 32768);
        drawer::Application app(std::filesystem::path(executable).parent_path());
        g_application = &app;
        if (smoke && !app.SmokeCheck()) throw std::runtime_error("Case smoke check failed");
        ShowWindow(window, SW_SHOWDEFAULT);
        UpdateWindow(window);
        bool done = false;
        int frames = 0;
        while (!done && !app.ShouldExit()) {
            MSG message;
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
                if (message.message == WM_QUIT) {
                    result = static_cast<int>(message.wParam);
                    done = true;
                }
            }
            if (done) break;
            if (g_minimized) {
                Sleep(10);
                continue;
            }
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            app.Frame();
            ImGui::Render();
            float clear[] = {.07f, .09f, .13f, 1};
            auto* target = g_target.Get();
            g_context->OMSetRenderTargets(1, &target, nullptr);
            g_context->ClearRenderTargetView(target, clear);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            if (smoke && frames == 10 && !screenshot.empty()) Capture(screenshot);
            Check(g_swap->Present(1, 0), "Present");
            if (smoke && ++frames >= 12) done = true;
        }
        g_application = nullptr;
    } catch (const std::exception& error) {
        result = 1;
        g_application = nullptr;
        if (std::wstring(GetCommandLineW()).find(L"--smoke-test") == std::wstring::npos)
            MessageBoxA(window, error.what(), "Drawer startup error", MB_OK | MB_ICONERROR);
        else
            OutputDebugStringA(error.what());
    }
    if (dx_ready) ImGui_ImplDX11_Shutdown();
    if (win32_ready) ImGui_ImplWin32_Shutdown();
    if (ImGui::GetCurrentContext()) ImGui::DestroyContext();
    g_target.Reset();
    g_swap.Reset();
    g_context.Reset();
    g_device.Reset();
    if (window) DestroyWindow(window);
    UnregisterClassW(L"DrawerWindow", instance);
    return result;
}
