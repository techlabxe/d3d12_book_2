#include "Swapchain.h"

Swapchain::Swapchain(
  ComPtr<IDXGISwapChain1> swapchain,
  std::shared_ptr<DescriptorManager>& heapRTV,
  bool useHDR)
{
  swapchain.As(&m_swapchain); // IDXGISwapChain4 取得
  m_swapchain->GetDesc1(&m_desc);

  ComPtr<ID3D12Device> device;
  swapchain->GetDevice(IID_PPV_ARGS(&device));

  m_images.resize(m_desc.BufferCount);
  m_imageRTV.resize(m_desc.BufferCount);
  m_fences.resize(m_desc.BufferCount);
  m_fenceValues.resize(m_desc.BufferCount);
  m_waitEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

  HRESULT hr;
  for (UINT i = 0; i < m_desc.BufferCount; ++i)
  {
    hr = device->CreateFence(
      0, D3D12_FENCE_FLAG_NONE,
      IID_PPV_ARGS(&m_fences[i]));
    ThrowIfFailed(hr, "CreateFence 失敗");

    m_imageRTV[i] = heapRTV->Alloc();

    // Swapchain イメージの RTV 生成.
    hr = m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_images[i]));
    ThrowIfFailed(hr, "GetBuffer() 失敗");
    device->CreateRenderTargetView(m_images[i].Get(), nullptr, m_imageRTV[i]);
  }

  // フォーマットに応じてカラースペースを設定.
  DXGI_COLOR_SPACE_TYPE colorSpace;
  switch (m_desc.Format)
  {
  default:
    colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
    break;
  case DXGI_FORMAT_R16G16B16A16_FLOAT:
    colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
    break;
  case DXGI_FORMAT_R10G10B10A2_UNORM:
    colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
    break;
  }
  m_swapchain->SetColorSpace1(colorSpace);

  if (useHDR)
  {
    SetMetadata();
  }
}

Swapchain::~Swapchain() {
  BOOL isFullScreen;
  m_swapchain->GetFullscreenState(&isFullScreen, nullptr);
  if (isFullScreen)
  {
    m_swapchain->SetFullscreenState(FALSE, nullptr);
  }
  CloseHandle(m_waitEvent);
}

DescriptorHandle Swapchain::GetCurrentRTV() const
{
  return m_imageRTV[GetCurrentBackBufferIndex()];
}

HRESULT Swapchain::Present(UINT SyncInterval, UINT Flags)
{
  return m_swapchain->Present(SyncInterval, Flags);
}


void Swapchain::WaitPreviousFrame(ComPtr<ID3D12CommandQueue> commandQueue, int frameIndex, DWORD timeout)
{
  auto fence = m_fences[frameIndex];
  // 現在のフェンスに GPU が到達後設定される値をセット.
  auto value = ++m_fenceValues[frameIndex];
  commandQueue->Signal(fence.Get(), value);

  // 次フレームで処理するコマンドの実行完了を待機する.
  auto nextIndex = GetCurrentBackBufferIndex();
  auto finishValue = m_fenceValues[nextIndex];
  fence = m_fences[nextIndex];
  value = fence->GetCompletedValue();
  if (value < finishValue)
  {
    // 未完了のためイベントで待機.
    fence->SetEventOnCompletion(finishValue, m_waitEvent);
    WaitForSingleObject(m_waitEvent, timeout);
  }
}

void Swapchain::ResizeBuffers(UINT width, UINT height)
{
  // リサイズのためにいったん解放.
  for (auto& v : m_images) {
    v.Reset();
  }
  HRESULT hr = m_swapchain->ResizeBuffers(
    m_desc.BufferCount,
    width, height, m_desc.Format, m_desc.Flags
  );
  ThrowIfFailed(hr, "ResizeBuffers 失敗");

  // イメージを取り直して RTV を再生成.
  ComPtr<ID3D12Device> device;
  m_swapchain->GetDevice(IID_PPV_ARGS(&device));
  for (UINT i = 0; i < m_desc.BufferCount; ++i) {
    m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_images[i]));
    device->CreateRenderTargetView(
      m_images[i].Get(),
      nullptr,
      m_imageRTV[i]);
  }
}

CD3DX12_RESOURCE_BARRIER Swapchain::GetBarrierToRenderTarget()
{
  return CD3DX12_RESOURCE_BARRIER::Transition(
    m_images[GetCurrentBackBufferIndex()].Get(),
    D3D12_RESOURCE_STATE_PRESENT,
    D3D12_RESOURCE_STATE_RENDER_TARGET);
}
CD3DX12_RESOURCE_BARRIER Swapchain::GetBarrierToPresent()
{
  return CD3DX12_RESOURCE_BARRIER::Transition(
    m_images[GetCurrentBackBufferIndex()].Get(),
    D3D12_RESOURCE_STATE_RENDER_TARGET,
    D3D12_RESOURCE_STATE_PRESENT);
}


void Swapchain::SetMetadata()
{
  struct DisplayChromacities
  {
    float RedX;
    float RedY;
    float GreenX;
    float GreenY;
    float BlueX;
    float BlueY;
    float WhiteX;
    float WhiteY;
  } DisplayChromacityList[] = {
    { 0.64000f, 0.33000f, 0.30000f, 0.60000f, 0.15000f, 0.06000f, 0.31270f, 0.32900f }, // Rec709 
    { 0.70800f, 0.29200f, 0.17000f, 0.79700f, 0.13100f, 0.04600f, 0.31270f, 0.32900f }, // Rec2020
  };
  int useIndex = 0;
  if (m_desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT)
  {
    useIndex = 1;
  }
  const auto& chroma = DisplayChromacityList[useIndex];
  DXGI_HDR_METADATA_HDR10 HDR10MetaData{};
  HDR10MetaData.RedPrimary[0] = UINT16(chroma.RedX * 50000.0f);
  HDR10MetaData.RedPrimary[1] = UINT16(chroma.RedY * 50000.0f);
  HDR10MetaData.GreenPrimary[0] = UINT16(chroma.GreenX * 50000.0f);
  HDR10MetaData.GreenPrimary[1] = UINT16(chroma.GreenY * 50000.0f);
  HDR10MetaData.BluePrimary[0] = UINT16(chroma.BlueX * 50000.0f);
  HDR10MetaData.BluePrimary[1] = UINT16(chroma.BlueY * 50000.0f);
  HDR10MetaData.WhitePoint[0] = UINT16(chroma.WhiteX * 50000.0f);
  HDR10MetaData.WhitePoint[1] = UINT16(chroma.WhiteY * 50000.0f);
  HDR10MetaData.MaxMasteringLuminance = UINT(1000.0f * 10000.0f);
  HDR10MetaData.MinMasteringLuminance = UINT(0.001f * 10000.0f);
  HDR10MetaData.MaxContentLightLevel = UINT16(2000.0f);
  HDR10MetaData.MaxFrameAverageLightLevel = UINT16(500.0f);
  m_swapchain->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(HDR10MetaData), &HDR10MetaData);
}

bool Swapchain::IsFullScreen() const
{
  BOOL fullscreen;
  if (FAILED(m_swapchain->GetFullscreenState(&fullscreen, nullptr)))
  {
    fullscreen = FALSE;
  }
  return fullscreen == TRUE;
}
void Swapchain::ResizeTarget(const DXGI_MODE_DESC *pNewTargetParameters)
{
  m_swapchain->ResizeTarget(pNewTargetParameters);
}
void Swapchain::SetFullScreen(bool toFullScreen)
{
  if (toFullScreen)
  {
    ComPtr<IDXGIOutput> output;
    m_swapchain->GetContainingOutput(&output);
    if (output)
    {
      DXGI_OUTPUT_DESC desc{};
      output->GetDesc(&desc);
    }

    m_swapchain->SetFullscreenState(TRUE, /*output.Get()*/nullptr);
  }
  else
  {
    m_swapchain->SetFullscreenState(FALSE, nullptr);
  }
}