#pragma once
#include <stdexcept>
#include <d3d12.h>
#include <DirectXMath.h>
#include "d3dx12.h"

#define STRINGFY(s)  #s
#define TO_STRING(x) STRINGFY(x)
#define FILE_PREFIX __FILE__ "(" TO_STRING(__LINE__) "): " 
#define ThrowIfFailed(hr, msg) book_util::CheckResultCodeD3D12( hr, FILE_PREFIX msg)

namespace book_util
{
  class DX12Exception : public std::runtime_error
  {
  public:
    DX12Exception(const std::string& msg) : std::runtime_error(msg.c_str()) {
    }
  };

  inline void CheckResultCodeD3D12(HRESULT hr, const std::string& errorMsg)
  {
    if (FAILED(hr))
    {
      throw DX12Exception(errorMsg);
    }
  }

  inline UINT RoundupConstantBufferSize(UINT size)
  {
    size = (size + 255) & ~255;
    return size;
  }

  inline CD3DX12_RASTERIZER_DESC CreateTeapotModelRasterizerDesc()
  {
    // Teapot �̃��f���͔����v���тŃf�[�^����`����Ă��邽��.
    auto desc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    desc.FrontCounterClockwise = true;
    return desc;
  }

  inline D3D12_GRAPHICS_PIPELINE_STATE_DESC CreateDefaultPsoDesc(
    DXGI_FORMAT targetFormat,
    Microsoft::WRL::ComPtr<ID3DBlob> vs, Microsoft::WRL::ComPtr<ID3DBlob> ps,
    CD3DX12_RASTERIZER_DESC rasterizerDesc,
    const D3D12_INPUT_ELEMENT_DESC* inputElementDesc, UINT inputElementDescCount,
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig
  )
  {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    // �V�F�[�_�[�̃Z�b�g
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
    // �u�����h�X�e�[�g�ݒ�
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    // ���X�^���C�U�[�X�e�[�g
    psoDesc.RasterizerState = rasterizerDesc;

    // �o�͐��1�^�[�Q�b�g
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = targetFormat;
    // �f�v�X�o�b�t�@�̃t�H�[�}�b�g��ݒ�
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.InputLayout = { inputElementDesc, inputElementDescCount };

    // ���[�g�V�O�l�`���̃Z�b�g
    psoDesc.pRootSignature = rootSig.Get();
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    // �}���`�T���v���ݒ�
    psoDesc.SampleDesc = { 1,0 };
    psoDesc.SampleMask = UINT_MAX; // �����Y���ƊG���o�Ȃ����x�����o�Ȃ��̂Œ���.
    return psoDesc;
  }

  inline Microsoft::WRL::ComPtr<ID3D12Resource1> CreateBufferOnUploadHeap(
    Microsoft::WRL::ComPtr<ID3D12Device> device, UINT bufferSize, const void* data = nullptr)
  {
    Microsoft::WRL::ComPtr<ID3D12Resource1> ret;
    HRESULT hr;
    const auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    const auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
    hr = device->CreateCommittedResource(
      &heapProps,
      D3D12_HEAP_FLAG_NONE,
      &resDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr,
      IID_PPV_ARGS(&ret)
    );
    ThrowIfFailed(hr, "CreateCommittedResource failed.");
    if (data != nullptr)
    {
      void* mapped;
      hr = ret->Map(0, nullptr, &mapped);
      ThrowIfFailed(hr, "Map failed.");
      if (mapped) {
        memcpy(mapped, data, bufferSize);
        ret->Unmap(0, nullptr);
      }
    }
    return ret;
  }

  inline DirectX::XMFLOAT4 toFloat4(const DirectX::XMFLOAT3& v, float w)
  {
    return DirectX::XMFLOAT4(
      v.x, v.y, v.z, w
    );
  }

  inline std::wstring ConvertWstring(const std::string& str)
  {
    std::vector<wchar_t> buf;
    int len = int(str.length());
    if (str.empty())
    {
      return std::wstring();
    }

    buf.resize(len * 4);
    MultiByteToWideChar(932, 0, str.data(), -1, buf.data(), int(buf.size()));

    return std::wstring(buf.data());
  }
}

