#include "BitMotor/Render/Texture.h"

#include "BitMotor/Core/Log.h"
#include "Render/ImageInternal.h"
#include "Render/RendererInternal.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstring>

namespace BitMotor {

namespace Detail {

namespace {
// Every texture alive, so the renderer can upload them when it starts and release them when it stops.
std::vector<TextureData*> Alive;
} // namespace

TextureData::TextureData() { Alive.push_back(this); }

TextureData::~TextureData() {
    std::erase(Alive, this);
    if (Gpu && GpuDevice()) SDL_ReleaseGPUTexture(GpuDevice(), Gpu);
}

namespace {

// A RenderTexture's image: drawable and sampled, cleared to transparent so it never shows garbage.
SDL_GPUTexture* MakeTarget(SDL_GPUDevice* device, TextureData& texture) {
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    info.width = Uint32(texture.Width);
    info.height = Uint32(texture.Height);
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    SDL_GPUTexture* gpu = SDL_CreateGPUTexture(device, &info);
    if (!gpu) {
        Log::Error("RenderTexture ({}x{}): {}", texture.Width, texture.Height, SDL_GetError());
        return nullptr;
    }
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUColorTargetInfo target{};
    target.texture = gpu;
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;
    SDL_EndGPURenderPass(SDL_BeginGPURenderPass(commands, &target, 1, nullptr));
    SDL_SubmitGPUCommandBuffer(commands);
    texture.Gpu = gpu;
    return gpu;
}

} // namespace

SDL_GPUTexture* GpuTexture(TextureData& texture) {
    SDL_GPUDevice* device = GpuDevice();
    if (texture.Gpu || !device) return texture.Gpu;
    if (texture.Target) return MakeTarget(device, texture);
    if (texture.Pixels.empty()) return nullptr; // it was already on a device that stopped

    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = Uint32(texture.Width);
    info.height = Uint32(texture.Height);
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    SDL_GPUTexture* gpu = SDL_CreateGPUTexture(device, &info);
    if (!gpu) {
        Log::Error("SDL_CreateGPUTexture ({}x{}): {}", texture.Width, texture.Height, SDL_GetError());
        return nullptr;
    }

    SDL_GPUTransferBufferCreateInfo bufferInfo{};
    bufferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    bufferInfo.size = Uint32(texture.Pixels.size());
    SDL_GPUTransferBuffer* buffer = SDL_CreateGPUTransferBuffer(device, &bufferInfo);
    void* mapped = buffer ? SDL_MapGPUTransferBuffer(device, buffer, false) : nullptr;
    if (!mapped) {
        Log::Error("Cannot upload a texture: {}", SDL_GetError());
        if (buffer) SDL_ReleaseGPUTransferBuffer(device, buffer);
        SDL_ReleaseGPUTexture(device, gpu);
        return nullptr;
    }
    std::memcpy(mapped, texture.Pixels.data(), texture.Pixels.size());
    SDL_UnmapGPUTransferBuffer(device, buffer);

    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
    SDL_GPUTextureTransferInfo source{};
    source.transfer_buffer = buffer;
    source.pixels_per_row = Uint32(texture.Width);
    source.rows_per_layer = Uint32(texture.Height);
    SDL_GPUTextureRegion destination{};
    destination.texture = gpu;
    destination.w = Uint32(texture.Width);
    destination.h = Uint32(texture.Height);
    destination.d = 1;
    SDL_UploadToGPUTexture(copy, &source, &destination, false);
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(commands);
    SDL_ReleaseGPUTransferBuffer(device, buffer); // SDL keeps it until the copy is done

    texture.Gpu = gpu;
    texture.Pixels = {}; // the GPU has them now
    return gpu;
}

void UploadTextures() {
    for (TextureData* texture : Alive) GpuTexture(*texture);
}

void ReleaseTextures() {
    SDL_GPUDevice* device = GpuDevice();
    for (TextureData* texture : Alive) {
        if (texture->Gpu && device) SDL_ReleaseGPUTexture(device, texture->Gpu);
        texture->Gpu = nullptr;
    }
}

} // namespace Detail

Texture Texture::FromPixels(int width, int height, std::span<const std::uint8_t> rgba, Filter filter) {
    if (width <= 0 || height <= 0 || rgba.size() != std::size_t(width) * std::size_t(height) * 4) {
        Log::Error("Texture::FromPixels: {}x{} needs {} bytes, got {}", width, height,
                   std::size_t(std::max(width, 0)) * std::size_t(std::max(height, 0)) * 4, rgba.size());
        return {};
    }
    Texture texture;
    texture.Data = std::make_shared<Detail::TextureData>();
    texture.Data->Width = width;
    texture.Data->Height = height;
    texture.Data->Look = filter;
    texture.Data->Pixels.assign(rgba.begin(), rgba.end());
    Detail::GpuTexture(*texture.Data); // now if there is a device; otherwise when it starts
    return texture;
}

Texture Texture::Load(const std::filesystem::path& path, Filter filter) {
    Detail::Image image = Detail::LoadImage(path);
    if (!image) return {};
    return FromPixels(image.Width, image.Height, image.Pixels, filter);
}

void Texture::SetNormals(const Texture& normals) const {
    if (!Data) return;
    if (normals && (normals.Width() != Width() || normals.Height() != Height()))
        Log::Warn("Texture::SetNormals: the normal map is {}x{}, the texture {}x{}", normals.Width(), normals.Height(),
                  Width(), Height());
    if (normals.Data == Data) return; // a texture cannot be its own normal map
    Data->Normals = normals;
}

Texture Texture::Normals() const { return Data ? Data->Normals : Texture(); }

int Texture::Width() const { return Data ? Data->Width : 0; }
int Texture::Height() const { return Data ? Data->Height : 0; }
Filter Texture::GetFilter() const { return Data ? Data->Look : Filter::Nearest; }

} // namespace BitMotor
