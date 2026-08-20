#pragma once

#include "V11ResourceIds.h"
#include "../core/logging/Logger.hpp"

#include <Windows.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <vector>

namespace Tutones::UI
{
    class ThemeTexture final
    {
    public:
        ThemeTexture() = default;
        ~ThemeTexture() { Reset(); }
        ThemeTexture(const ThemeTexture&) = delete;
        ThemeTexture& operator=(const ThemeTexture&) = delete;

        bool LoadFile(const std::filesystem::path& path) noexcept
        {
            return LoadDecoded([&](IWICImagingFactory* factory, IWICBitmapDecoder** decoder) {
                return factory->CreateDecoderFromFilename(
                    path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, decoder);
            });
        }

        bool LoadEmbeddedBanner() noexcept
        {
            HMODULE module{};
            if (!::GetModuleHandleExW(
                    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCWSTR>(&g_ModuleAnchor), &module))
                return false;

            const HRSRC resource = ::FindResourceW(
                module, MAKEINTRESOURCEW(IDR_V11_BANNER_COMPOSITE), MAKEINTRESOURCEW(10));
            if (!resource)
                return false;
            const HGLOBAL loaded = ::LoadResource(module, resource);
            const DWORD size = ::SizeofResource(module, resource);
            const void* data = loaded ? ::LockResource(loaded) : nullptr;
            if (!data || size == 0)
                return false;

            return LoadDecoded([&](IWICImagingFactory* factory, IWICBitmapDecoder** decoder) {
                Microsoft::WRL::ComPtr<IStream> stream;
                HRESULT hr = ::CreateStreamOnHGlobal(nullptr, TRUE, stream.GetAddressOf());
                if (FAILED(hr)) return hr;
                ULONG written{};
                hr = stream->Write(data, size, &written);
                if (FAILED(hr) || written != size) return FAILED(hr) ? hr : STG_E_WRITEFAULT;
                LARGE_INTEGER zero{};
                hr = stream->Seek(zero, STREAM_SEEK_SET, nullptr);
                if (FAILED(hr)) return hr;
                return factory->CreateDecoderFromStream(
                    stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, decoder);
            });
        }

        void Reset() noexcept
        {
            if (!m_Texture)
                return;
            if (m_Context && ImGui::GetCurrentContext() == m_Context)
                ImGui::UnregisterUserTexture(m_Texture.get());
            m_Texture.reset();
            m_Context = nullptr;
        }

        [[nodiscard]] ImTextureRef Ref() const noexcept
        {
            return m_Texture ? m_Texture->GetTexRef() : ImTextureRef{};
        }
        [[nodiscard]] bool Valid() const noexcept { return m_Texture != nullptr; }

    private:
        template <typename OpenDecoder>
        bool LoadDecoded(OpenDecoder&& openDecoder) noexcept
        {
            if (!ImGui::GetCurrentContext())
                return false;

            const HRESULT com = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if (FAILED(com) && com != RPC_E_CHANGED_MODE)
                return false;
            const bool uninit = SUCCEEDED(com);

            using Microsoft::WRL::ComPtr;
            ComPtr<IWICImagingFactory> factory;
            ComPtr<IWICBitmapDecoder> decoder;
            ComPtr<IWICBitmapFrameDecode> frame;
            ComPtr<IWICFormatConverter> converter;

            HRESULT hr = ::CoCreateInstance(
                CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(factory.GetAddressOf()));
            if (SUCCEEDED(hr)) hr = openDecoder(factory.Get(), decoder.GetAddressOf());
            if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, frame.GetAddressOf());
            if (SUCCEEDED(hr)) hr = factory->CreateFormatConverter(converter.GetAddressOf());
            if (SUCCEEDED(hr))
            {
                hr = converter->Initialize(
                    frame.Get(), GUID_WICPixelFormat32bppRGBA,
                    WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
            }

            UINT width{}, height{};
            if (SUCCEEDED(hr)) hr = converter->GetSize(&width, &height);
            constexpr UINT MaxDimension = 8192;
            if (SUCCEEDED(hr) && (width == 0 || height == 0 || width > MaxDimension || height > MaxDimension))
                hr = E_INVALIDARG;

            std::vector<std::uint8_t> pixels;
            const std::size_t pitch = static_cast<std::size_t>(width) * 4u;
            const std::size_t total = pitch * static_cast<std::size_t>(height);
            if (SUCCEEDED(hr) && (total == 0 || total > 256u * 1024u * 1024u || total > UINT_MAX))
                hr = E_OUTOFMEMORY;
            if (SUCCEEDED(hr))
            {
                pixels.resize(total);
                hr = converter->CopyPixels(nullptr, static_cast<UINT>(pitch), static_cast<UINT>(total), pixels.data());
            }

            converter.Reset();
            frame.Reset();
            decoder.Reset();
            factory.Reset();
            if (uninit) ::CoUninitialize();
            if (FAILED(hr) || pixels.empty())
                return false;

            auto texture = std::make_unique<ImTextureData>();
            texture->Create(ImTextureFormat_RGBA32, static_cast<int>(width), static_cast<int>(height));
            if (!texture->GetPixels())
                return false;
            std::memcpy(texture->GetPixels(), pixels.data(), total);
            Reset();
            ImGui::RegisterUserTexture(texture.get());
            m_Context = ImGui::GetCurrentContext();
            m_Texture = std::move(texture);
            return true;
        }

        inline static int g_ModuleAnchor{};
        ImGuiContext* m_Context{};
        std::unique_ptr<ImTextureData> m_Texture;
    };
}
