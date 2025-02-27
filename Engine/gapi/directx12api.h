#pragma once

#include "abstractgraphicsapi.h"

#include <memory>
#include <cstdint>
#include <vector>

namespace Tempest {

class DirectX12Api : public AbstractGraphicsApi {
  public:
    explicit DirectX12Api(ApiFlags f=ApiFlags::NoFlags);
    virtual ~DirectX12Api();

    std::vector<Props> devices() const override;

  protected:
    Device*        createDevice(const char* gpuName) override;
    void           destroy(Device* d) override;

    Swapchain*     createSwapchain(SystemApi::Window* w, Device *d) override;

    PPipelineLay   createPipelineLayout(Device *d, const Shader*const* sh, size_t count) override;
    PPipeline      createPipeline(Device* d, const RenderState &st, Topology tp, const PipelineLay& ulayImpl,
                                  const Shader*const* shaders, size_t count) override;
    PCompPipeline  createComputePipeline(Device* d,
                                         const PipelineLay &ulayImpl,
                                         Shader* sh) override;

    PShader        createShader(Device *d, const void* source, size_t src_size) override;

    Fence*         createFence(Device *d) override;

    PBuffer        createBuffer(Device* d, const void *mem, size_t count, size_t size, size_t alignedSz, MemUsage usage, BufferHeap flg) override;

    PTexture       createTexture(Device* d, const Pixmap& p, TextureFormat frm, uint32_t mips) override;
    PTexture       createTexture(Device* d, const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm) override;
    PTexture       createStorage(Device* d, const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm) override;
    PTexture       createStorage(Device* d, const uint32_t w, const uint32_t h, const uint32_t depth, uint32_t mips, TextureFormat frm) override;

    AccelerationStructure* createBottomAccelerationStruct(Device* d,
                                                          Buffer* vbo, size_t vboSz, size_t stride,
                                                          Buffer* ibo, size_t iboSz, size_t offset, Detail::IndexClass icls) override;
    AccelerationStructure* createTopAccelerationStruct(Device* d, const RtInstance* inst, AccelerationStructure*const* as, size_t geomSize) override;

    void           readPixels(Device* d, Pixmap& out, const PTexture t,
                              TextureFormat frm, const uint32_t w, const uint32_t h, uint32_t mip, bool storageImg) override;
    void           readBytes(Device* d, Buffer* buf, void* out, size_t size) override;

    Desc*          createDescriptors(Device* d, PipelineLay& layP) override;

    CommandBuffer* createCommandBuffer(Device* d) override;

    void           present  (Device *d, Swapchain* sw) override;
    void           submit   (Device *d, CommandBuffer* cmd,  Fence* doneCpu) override;

    void           getCaps  (Device *d,Props& caps) override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    PTexture       createCompressedTexture(Device* d, const Pixmap& p, TextureFormat frm, uint32_t mipCnt);
  };

}

