#include "SampleImGui.h"
#include "imgui_helper.h"

#include "imgui.h"
#include "examples/imgui_impl_dx12.h"
#include "examples/imgui_impl_win32.h"

void SampleImGui::Prepare()
{
  SetTitle("ImGui Sample");

  auto descriptor = m_heap->Alloc();
  CD3DX12_CPU_DESCRIPTOR_HANDLE hCpu(descriptor);
  CD3DX12_GPU_DESCRIPTOR_HANDLE hGpu(descriptor);

  imgui_helper::PrepareImGui(
    m_hwnd,
    m_device.Get(),
    m_surfaceFormat,
    FrameBufferCount,
    hCpu, hGpu
  );
}

void SampleImGui::Cleanup()
{
  imgui_helper::CleanupImGui();
}

void SampleImGui::Render()
{
  UpdateImGui();

  m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();

  m_commandAllocators[m_frameIndex]->Reset();
  m_commandList->Reset(
    m_commandAllocators[m_frameIndex].Get(),
    nullptr
  );

  // �X���b�v�`�F�C���\���\���烌���_�[�^�[�Q�b�g�`��\��
  auto barrierToRT = m_swapchain->GetBarrierToRenderTarget();
  m_commandList->ResourceBarrier(1, &barrierToRT);

  auto rtv = m_swapchain->GetCurrentRTV();
  auto dsv = m_defaultDepthDSV;

  // �J���[�o�b�t�@(�����_�[�^�[�Q�b�g�r���[)�̃N���A
  m_commandList->ClearRenderTargetView(rtv, m_clearColor, 0, nullptr);

  // �f�v�X�o�b�t�@(�f�v�X�X�e���V���r���[)�̃N���A
  m_commandList->ClearDepthStencilView(
    dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

  // �`�����Z�b�g
  m_commandList->OMSetRenderTargets(1, &(D3D12_CPU_DESCRIPTOR_HANDLE)rtv,
    FALSE, &(D3D12_CPU_DESCRIPTOR_HANDLE)dsv);

  ID3D12DescriptorHeap* heaps[] = { m_heap->GetHeap().Get() };
  m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);

  RenderImGui();

  // �����_�[�^�[�Q�b�g����X���b�v�`�F�C���\���\��
  auto barrierToPresent = m_swapchain->GetBarrierToPresent();
  m_commandList->ResourceBarrier(1, &barrierToPresent);

  m_commandList->Close();

  ID3D12CommandList* lists[] = { m_commandList.Get() };

  m_commandQueue->ExecuteCommandLists(1, lists);

  m_swapchain->Present(1, 0);
  m_swapchain->WaitPreviousFrame(m_commandQueue, m_frameIndex, GpuWaitTimeout);

}

void SampleImGui::UpdateImGui()
{
  ImGui_ImplDX12_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();

  static float f = 0.0f;
  static int counter = 0;

  ImGui::Begin("Information");
  ImGui::Text("Hello,ImGui world");
  ImGui::Text("Framerate(avg) %.3f ms/frame (%.1f FPS)",
    1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
  if (ImGui::Button("Button"))
  {
    // �{�^�����������ꂽ�Ƃ��̏���.
  }

  ImGui::SliderFloat("Factor", &m_factor, 0.0f, 100.0f);
  ImGui::ColorEdit4("ClearColor", m_clearColor, ImGuiColorEditFlags_PickerHueWheel);
  // ������̓J���[�s�b�J�[���W�J�ς�.
  ImGui::ColorPicker4("ClearColor", m_clearColor);
  ImGui::End();
}

void SampleImGui::RenderImGui()
{
  ImGui::Render();
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());
}