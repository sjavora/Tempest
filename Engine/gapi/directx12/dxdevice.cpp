#if defined(TEMPEST_BUILD_DIRECTX12)

#include <Tempest/Log>
#include <iostream>

#include "guid.h"
#include "dxdevice.h"

#include "dxshader.h"
#include "dxpipeline.h"
#include "builtin_shader.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxDevice::DxDevice(IDXGIAdapter1& adapter, const ApiEntry& dllApi)
  :dllApi(dllApi) {
  dxAssert(dllApi.D3D12CreateDevice(&adapter, D3D_FEATURE_LEVEL_11_0, uuid<ID3D12Device>(), reinterpret_cast<void**>(&device)));

  ComPtr<ID3D12InfoQueue> pInfoQueue;
  if(SUCCEEDED(device->QueryInterface(uuid<ID3D12InfoQueue>(),reinterpret_cast<void**>(&pInfoQueue)))) {
    // Suppress messages based on their severity level
    D3D12_MESSAGE_SEVERITY severities[] = {
      D3D12_MESSAGE_SEVERITY_INFO
      };
    D3D12_MESSAGE_ID denyIds[] = {
      // I'm really not sure how to avoid this message.
      D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
      D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
      // blit shader uses no vbo
      D3D12_MESSAGE_ID_CREATEINPUTLAYOUT_EMPTY_LAYOUT,
      };
    D3D12_INFO_QUEUE_FILTER filter = {};
    filter.DenyList.NumSeverities = _countof(severities);
    filter.DenyList.pSeverityList = severities;
    filter.DenyList.NumIDs        = _countof(denyIds);
    filter.DenyList.pIDList       = denyIds;

    dxAssert(pInfoQueue->PushStorageFilter(&filter));

    pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
    pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
    pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);
    }

  DXGI_ADAPTER_DESC desc={};
  adapter.GetDesc(&desc);
  getProp(adapter,*device,props);

  D3D12_COMMAND_QUEUE_DESC queueDesc = {};
  queueDesc.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;
  queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  dxAssert(device->CreateCommandQueue(&queueDesc, uuid<ID3D12CommandQueue>(), reinterpret_cast<void**>(&cmdQueue)));

  allocator.setDevice(*this);

  dxAssert(device->CreateFence(DxFence::Waiting, D3D12_FENCE_FLAG_NONE,
                               uuid<ID3D12Fence>(),
                               reinterpret_cast<void**>(&idleFence)));
  idleEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

  static bool internalShaders = true;

  if(internalShaders) {
    {
    RenderState st;
    st.setZTestMode   (RenderState::ZTestMode::Always);
    st.setCullFaceMode(RenderState::CullMode::NoCull);

    auto blitVs = DSharedPtr<DxShader*>(new DxShader(blit_vert_sprv,sizeof(blit_vert_sprv)));
    auto blitFs = DSharedPtr<DxShader*>(new DxShader(blit_frag_sprv,sizeof(blit_frag_sprv)));

    DxShader* blitSh[2] = {blitVs.handler, blitFs.handler};
    blitLayout  = DSharedPtr<DxPipelineLay*>(new DxPipelineLay(*this,&blitFs.handler->lay));
    blit        = DSharedPtr<DxPipeline*>   (new DxPipeline   (*this,st,Triangles,*blitLayout.handler,blitSh,2));
    }

    {
    auto copyCs = DSharedPtr<DxShader*>(new DxShader(copy_comp_sprv,sizeof(copy_comp_sprv)));
    copyLayout  = DSharedPtr<DxPipelineLay*> (new DxPipelineLay(*this,&copyCs.handler->lay));
    copy        = DSharedPtr<DxCompPipeline*>(new DxCompPipeline(*this,*copyLayout.handler,*copyCs.handler));
    }
    {
    auto copyCs = DSharedPtr<DxShader*>(new DxShader(copy_s_comp_sprv,sizeof(copy_s_comp_sprv)));
    copyS       = DSharedPtr<DxCompPipeline*>(new DxCompPipeline(*this,*copyLayout.handler,*copyCs.handler));
    }
    }

  data.reset(new DataMgr(*this));
  }

DxDevice::~DxDevice() {
  blit       = DSharedPtr<DxPipeline*>();
  blitLayout = DSharedPtr<DxPipelineLay*>();
  data.reset();
  CloseHandle(idleEvent);
  }

void DxDevice::getProp(IDXGIAdapter1& adapter, ID3D12Device& dev, AbstractGraphicsApi::Props& prop) {
  DXGI_ADAPTER_DESC1 desc={};
  adapter.GetDesc1(&desc);
  return getProp(desc,dev,prop);
  }

void DxDevice::getProp(DXGI_ADAPTER_DESC1& desc, ID3D12Device& dev, AbstractGraphicsApi::Props& prop) {
  for(size_t i=0;i<sizeof(prop.name);++i)  {
    WCHAR c = desc.Description[i];
    if(c==0)
      break;
    if(('0'<=c && c<='9') || ('a'<=c && c<='z') || ('A'<=c && c<='Z') ||
       c=='(' || c==')' || c=='_' || c=='[' || c==']' || c=='{' || c=='}' || c==' ')
      prop.name[i] = char(c); else
      prop.name[i] = '?';
    }
  prop.name[sizeof(prop.name)-1]='\0';

  // https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/hardware-support-for-direct3d-12-0-formats
  // NOTE: TextureFormat::RGB32F is not supported, because of mip-maps
  static const TextureFormat smp[] = {TextureFormat::R8,   TextureFormat::RG8,   TextureFormat::RGBA8,
                                      TextureFormat::R16,  TextureFormat::RG16,  TextureFormat::RGBA16,
                                      TextureFormat::R32F, TextureFormat::RG32F, /*TextureFormat::RGB32F,*/ TextureFormat::RGBA32F,
                                      TextureFormat::DXT1, TextureFormat::DXT3,  TextureFormat::DXT5,
                                      TextureFormat::R11G11B10UF, TextureFormat::RGBA16F,
                                     };

  static const TextureFormat att[] = {TextureFormat::R8,   TextureFormat::RG8,   TextureFormat::RGBA8,
                                      TextureFormat::R16,  TextureFormat::RG16,  TextureFormat::RGBA16,
                                      TextureFormat::R32F, TextureFormat::RG32F, TextureFormat::RGBA32F,
                                      TextureFormat::R11G11B10UF, TextureFormat::RGBA16F,
                                     };

  static const TextureFormat sso[] = {TextureFormat::R8,   TextureFormat::RG8,   TextureFormat::RGBA8,
                                      TextureFormat::R16,  TextureFormat::RG16,  TextureFormat::RGBA16,
                                      TextureFormat::R32F, TextureFormat::RGBA32F,
                                      TextureFormat::R11G11B10UF, TextureFormat::RGBA16F,
                                     };

  static const TextureFormat ds[]  = {TextureFormat::Depth16, TextureFormat::Depth24x8, TextureFormat::Depth24S8, TextureFormat::Depth32F};

  uint64_t smpBit = 0, attBit = 0, dsBit = 0, storBit = 0;
  for(auto& i:smp)
    smpBit |= uint64_t(1) << uint64_t(i);
  for(auto& i:att)
    attBit |= uint64_t(1) << uint64_t(i);
  for(auto& i:ds) {
    dsBit  |= uint64_t(1) << uint64_t(i);
    smpBit |= uint64_t(1) << uint64_t(i);
    }
  for(auto& i:sso)
    storBit  |= uint64_t(1) << uint64_t(i);
  prop.setSamplerFormats(smpBit);
  prop.setAttachFormats (attBit);
  prop.setDepthFormats  (dsBit);
  prop.setStorageFormats(storBit);

  // TODO: buffer limits
  //prop.vbo.maxRange    = ;

  prop.ssbo.offsetAlign = 256;
  //prop.ssbo.maxRange    = size_t(prop.limits.maxStorageBufferRange);

  prop.ubo.maxRange      = D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT*4;
  prop.ubo.offsetAlign   = 256;

  prop.push.maxRange     = 256;

  prop.anisotropy        = true;
  prop.maxAnisotropy     = 16;
  prop.tesselationShader = false; // TODO: dxil compiller crashes

  prop.storeAndAtomicVs  = true;
  prop.storeAndAtomicFs  = true;

  prop.mrt.maxColorAttachments = D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;

  prop.compute.maxGroups.x    = D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
  prop.compute.maxGroups.y    = D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
  prop.compute.maxGroups.z    = D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;

  prop.compute.maxGroupSize.x = D3D12_CS_THREAD_GROUP_MAX_X;
  prop.compute.maxGroupSize.y = D3D12_CS_THREAD_GROUP_MAX_Y;
  prop.compute.maxGroupSize.z = D3D12_CS_THREAD_GROUP_MAX_Z;

  if(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
    prop.type = DeviceType::Cpu;
    } else {
    D3D12_FEATURE_DATA_ARCHITECTURE arch = {};
    if(SUCCEEDED(dev.CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE, &arch, sizeof(arch))))
      prop.type = arch.UMA ? DeviceType::Integrated : DeviceType::Discrete;
    }

  D3D12_FEATURE_DATA_D3D12_OPTIONS5 feature5 = {};
  if(SUCCEEDED(dev.CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &feature5, sizeof(feature5)))) {
    prop.raytracing.rayQuery = (feature5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED);
    //prop.raytracing.rayQuery = false; // TODO: dxil compiller
    }

  D3D12_FEATURE_DATA_D3D12_OPTIONS7 feature7 = {};
  if(SUCCEEDED(dev.CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &feature7, sizeof(feature7)))) {
    prop.meshlets.taskShader = false; // feature7.MeshShaderTier!=D3D12_MESH_SHADER_TIER_NOT_SUPPORTED;
    prop.meshlets.meshShader = false; // feature7.MeshShaderTier!=D3D12_MESH_SHADER_TIER_NOT_SUPPORTED;
    // https://microsoft.github.io/DirectX-Specs/d3d/MeshShader.html#dispatchmesh-api
    // ThreadGroupCountX*ThreadGroupCountY*ThreadGroupCountZ must not exceed 2^22.
    prop.meshlets.maxGroups.x = D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
    prop.meshlets.maxGroups.y = D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
    prop.meshlets.maxGroups.z = D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
    // https://microsoft.github.io/DirectX-Specs/d3d/MeshShader.html#numthreads
    // The number of threads can not exceed X * Y * Z = 128
    prop.meshlets.maxGroupSize.x = 128;
    prop.meshlets.maxGroupSize.y = 128;
    prop.meshlets.maxGroupSize.z = 128;
    }
  prop.bindless.nonUniformIndexing = true;  // SM5.1
  prop.bindless.nonUniformIndexing = false; // TEST: DirectX12.Bindless2
  }

void DxDevice::waitData() {
  data->wait();
  }

void Detail::DxDevice::waitIdle() {
  std::lock_guard<SpinLock> guard(syncCmdQueue);
  dxAssert(cmdQueue->Signal(idleFence.get(),DxFence::Ready));
  dxAssert(idleFence->SetEventOnCompletion(DxFence::Ready,idleEvent));
  WaitForSingleObjectEx(idleEvent, INFINITE, FALSE);
  dxAssert(idleFence->Signal(DxFence::Waiting));
  }

void DxDevice::submit(DxCommandBuffer& cmdBuffer, DxFence* sync) {
  sync->reset();

  std::lock_guard<SpinLock> guard(syncCmdQueue);
  ID3D12CommandList* cmd[] = {cmdBuffer.get()};
  cmdQueue->ExecuteCommandLists(1, cmd);
  sync->signal(*cmdQueue);
  }

#endif
