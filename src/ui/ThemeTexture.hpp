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
#include <string>
#include <utility>
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
            return LoadDecoded([&](
                IWICImagingFactory* factory,
                IWICBitmapDecoder** decoder,
                Microsoft::WRL::ComPtr<IStream>&) {
                return factory->CreateDecoderFromFilename(
                    path.c_str(),
                    nullptr,
                    GENERIC_READ,
                    WICDecodeMetadataCacheOnLoad,
                    decoder);
            });
        }

        bool LoadEmbeddedBanner() noexcept
        {
            HMODULE module{};
            if (!::GetModuleHandleExW(
                    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCWSTR>(&g_ModuleAnchor),
                    &module))
            {
                TUTONES_LOG_WARN("ui.theme.texture", "Could not resolve Tutones module for embedded banner");
                return false;
            }

            const HRSRC resource = ::FindResourceW(
                module,
                MAKEINTRESOURCEW(IDR_V11_BANNER_COMPOSITE),
                MAKEINTRESOURCEW(10));
            if (!resource)
            {
                TUTONES_LOG_WARN("ui.theme.texture", "Embedded banner resource was not found");
                return false;
            }

            const HGLOBAL loaded = ::LoadResource(module, resource);
            const DWORD size = ::SizeofResource(module, resource);
            const void* data = loaded ? ::LockResource(loaded) : nullptr;
            if (!data || size == 0)
            {
                TUTONES_LOG_WARN("ui.theme.texture", "Embedded banner resource could not be loaded");
                return false;
            }

            return LoadDecoded([&](
                IWICImagingFactory* factory,
                IWICBitmapDecoder** decoder,
                Microsoft::WRL::ComPtr<IStream>& sourceStream) {
                HRESULT hr = ::CreateStreamOnHGlobal(nullptr, TRUE, sourceStream.GetAddressOf());
                if (FAILED(hr))
                    return hr;

                ULONG written{};
                hr = sourceStream->Write(data, size, &written);
                if (FAILED(hr) || written != size)
                    return FAILED(hr) ? hr : STG_E_WRITEFAULT;

                LARGE_INTEGER zero{};
                hr = sourceStream->Seek(zero, STREAM_SEEK_SET, nullptr);
                if (FAILED(hr))
                    return hr;

                return factory->CreateDecoderFromStream(
                    sourceStream.Get(),
                    nullptr,
                    WICDecodeMetadataCacheOnLoad,
                    decoder);
            });
        }

        // ImGui 1.92 user textures are not backend resources owned by ImTextureData's
        // destructor. UnregisterUserTexture() only removes the pointer from the context;
        // it does not release the DX12 resource or return the SRV descriptor. Retired
        // textures therefore stay registered until the renderer backend has honored the
        // WantDestroy request and cleared TexID/BackendUserData.
        static void CollectGarbage() noexcept
        {
            auto* context = ImGui::GetCurrentContext();
            if (!context)
                return;

            for (auto it = g_RetiredTextures.begin(); it != g_RetiredTextures.end();)
            {
                if (!it->texture)
                {
                    it = g_RetiredTextures.erase(it);
                    continue;
                }

                if (it->context != context)
                {
                    ++it;
                    continue;
                }

                ImTextureData* texture = it->texture.get();
                const bool backendReleased = texture->Status == ImTextureStatus_Destroyed
                    && texture->TexID == ImTextureID_Invalid
                    && texture->BackendUserData == nullptr
                    && texture->QueueUserData == nullptr;
                if (!backendReleased)
                {
                    ++it;
                    continue;
                }

                if (texture->RefCount > 0)
                    ImGui::UnregisterUserTexture(texture);
                it = g_RetiredTextures.erase(it);
            }
        }

        void Reset() noexcept
        {
            CollectGarbage();

            if (m_Texture)
            {
                auto* currentContext = ImGui::GetCurrentContext();
                if (m_Context && currentContext == m_Context)
                {
                    const bool reachedBackend = m_Texture->TexID != ImTextureID_Invalid
                        || m_Texture->BackendUserData != nullptr
                        || m_Texture->QueueUserData != nullptr;

                    if (!reachedBackend)
                    {
                        if (m_Texture->RefCount > 0)
                            ImGui::UnregisterUserTexture(m_Texture.get());
                        m_Texture.reset();
                    }
                    else
                    {
                        // Keep the ImTextureData object and its CPU pixels alive. Dear ImGui
                        // deliberately waits NumFramesInFlight before the DX12 backend frees
                        // the GPU resource/descriptor. Deleting it here causes descriptor
                        // leaks and eventual crashes while rapidly changing vehicle previews.
                        m_Texture->WantDestroyNextFrame = true;
                        g_RetiredTextures.push_back(RetiredTexture{
                            m_Context,
                            std::move(m_Texture),
                        });
                    }
                }
                else
                {
                    // The owning ImGui context is already gone or has changed. Renderer
                    // shutdown/invalidations release backend state before context teardown;
                    // at this point only the CPU-side wrapper remains ours to delete.
                    m_Texture.reset();
                }
            }

            m_Context = nullptr;
            m_Width = 0;
            m_Height = 0;
        }

        [[nodiscard]] ImTextureRef Ref() const noexcept
        {
            return m_Texture ? m_Texture->GetTexRef() : ImTextureRef{};
        }

        [[nodiscard]] bool Valid() const noexcept { return m_Texture != nullptr; }
        [[nodiscard]] std::uint32_t Width() const noexcept { return m_Width; }
        [[nodiscard]] std::uint32_t Height() const noexcept { return m_Height; }

    private:
        struct RetiredTexture final
        {
            ImGuiContext* context{};
            std::unique_ptr<ImTextureData> texture{};
        };

        static void LogDecodeFailure(const char* stage, HRESULT result) noexcept
        {
            std::string message("Theme texture decode failed at ");
            message += stage ? stage : "unknown stage";
            message += " (HRESULT=";
            message += std::to_string(static_cast<long>(result));
            message += ')';
            TUTONES_LOG_WARN("ui.theme.texture", message);
        }

        template <typename OpenDecoder>
        bool LoadDecoded(OpenDecoder&& openDecoder) noexcept
        {
            if (!ImGui::GetCurrentContext())
                return false;

            CollectGarbage();

            const HRESULT com = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if (FAILED(com) && com != RPC_E_CHANGED_MODE)
            {
                LogDecodeFailure("COM initialization", com);
                return false;
            }
            const bool uninit = SUCCEEDED(com);

            using Microsoft::WRL::ComPtr;
            ComPtr<IWICImagingFactory> factory;
            ComPtr<IWICBitmapDecoder> decoder;
            ComPtr<IWICBitmapFrameDecode> frame;
            ComPtr<IWICFormatConverter> converter;
            ComPtr<IStream> sourceStream;

            HRESULT hr = S_OK;
            const char* stage = "create WIC imaging factory";
            bool decodedAsBgra = false;

            hr = ::CoCreateInstance(
                CLSID_WICImagingFactory,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(factory.GetAddressOf()));

            if (SUCCEEDED(hr))
            {
                stage = "open image decoder";
                hr = openDecoder(factory.Get(), decoder.GetAddressOf(), sourceStream);
            }

            if (SUCCEEDED(hr))
            {
                stage = "read image frame 0";
                hr = decoder->GetFrame(0, frame.GetAddressOf());
            }

            const auto initializeConverter = [&](REFWICPixelFormatGUID format) noexcept -> HRESULT
            {
                converter.Reset();
                HRESULT result = factory->CreateFormatConverter(converter.GetAddressOf());
                if (FAILED(result))
                    return result;

                return converter->Initialize(
                    frame.Get(),
                    format,
                    WICBitmapDitherTypeNone,
                    nullptr,
                    0.0,
                    WICBitmapPaletteTypeCustom);
            };

            if (SUCCEEDED(hr))
            {
                stage = "initialize RGBA converter";
                hr = initializeConverter(GUID_WICPixelFormat32bppRGBA);
                if (FAILED(hr))
                {
                    stage = "initialize BGRA converter fallback";
                    hr = initializeConverter(GUID_WICPixelFormat32bppBGRA);
                    decodedAsBgra = SUCCEEDED(hr);
                }
            }

            UINT width{};
            UINT height{};
            if (SUCCEEDED(hr))
            {
                stage = "query image dimensions";
                hr = converter->GetSize(&width, &height);
            }

            constexpr UINT MaxDimension = 8192;
            if (SUCCEEDED(hr)
                && (width == 0 || height == 0 || width > MaxDimension || height > MaxDimension))
            {
                stage = "validate image dimensions";
                hr = E_INVALIDARG;
            }

            std::vector<std::uint8_t> pixels;
            const std::size_t pitch = static_cast<std::size_t>(width) * 4u;
            const std::size_t total = pitch * static_cast<std::size_t>(height);
            if (SUCCEEDED(hr)
                && (total == 0 || total > 256u * 1024u * 1024u || total > UINT_MAX))
            {
                stage = "validate decoded image size";
                hr = E_OUTOFMEMORY;
            }

            if (SUCCEEDED(hr))
            {
                pixels.resize(total);
                stage = "copy decoded pixels";
                hr = converter->CopyPixels(
                    nullptr,
                    static_cast<UINT>(pitch),
                    static_cast<UINT>(total),
                    pixels.data());
            }

            // Release every WIC/COM object, including the embedded source stream, before
            // balancing CoInitializeEx. The stream must remain alive until CopyPixels finishes.
            converter.Reset();
            frame.Reset();
            decoder.Reset();
            sourceStream.Reset();
            factory.Reset();
            if (uninit)
                ::CoUninitialize();

            if (FAILED(hr) || pixels.empty())
            {
                LogDecodeFailure(stage, FAILED(hr) ? hr : E_FAIL);
                return false;
            }

            if (decodedAsBgra)
            {
                for (std::size_t i = 0; i + 3 < pixels.size(); i += 4)
                    std::swap(pixels[i], pixels[i + 2]);
            }

            auto texture = std::make_unique<ImTextureData>();
            texture->Create(
                ImTextureFormat_RGBA32,
                static_cast<int>(width),
                static_cast<int>(height));
            if (!texture->GetPixels())
            {
                TUTONES_LOG_WARN("ui.theme.texture", "ImGui texture pixel allocation failed");
                return false;
            }

            std::memcpy(texture->GetPixels(), pixels.data(), total);

            Reset();
            ImGui::RegisterUserTexture(texture.get());
            m_Context = ImGui::GetCurrentContext();
            m_Width = width;
            m_Height = height;
            m_Texture = std::move(texture);
            return true;
        }

        inline static int g_ModuleAnchor{};
        inline static std::vector<RetiredTexture> g_RetiredTextures{};
        ImGuiContext* m_Context{};
        std::unique_ptr<ImTextureData> m_Texture;
        std::uint32_t m_Width{};
        std::uint32_t m_Height{};
    };
}
