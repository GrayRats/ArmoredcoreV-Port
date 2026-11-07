#pragma once

#include <SDL.h>
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan.h>

#include <cstdint>

namespace acv::ui {

// Single-window Vulkan presentation for the ACV launcher. SDL owns the native
// window; this class owns all Vulkan objects created for it.
class VulkanRenderer {
 public:
  VulkanRenderer() = default;
  ~VulkanRenderer();
  VulkanRenderer(const VulkanRenderer&) = delete;
  VulkanRenderer& operator=(const VulkanRenderer&) = delete;

  void Initialize(SDL_Window* window);
  void NewFrame();
  // Returns false while minimized or when acquisition reports out-of-date.
  bool Render(ImDrawData* draw_data);
  static void Check(VkResult result, const char* operation);

 private:
  static void CheckBackendResult(VkResult result);
  static void CheckBackendStatus(const char* operation);
  void SelectDevice();
  void CreateDevice();
  void CreateWindowResources(int width, int height);
  void RebuildSwapchain(int width, int height);
  void ValidateWindowResources() const;
  void Shutdown() noexcept;

  SDL_Window* window_ = nullptr;
  VkInstance instance_ = VK_NULL_HANDLE;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
  VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  uint32_t queue_family_ = UINT32_MAX;
  VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
  ImGui_ImplVulkanH_Window window_data_{};
  bool sdl_backend_ = false;
  bool vulkan_backend_ = false;
  bool swapchain_rebuild_ = false;
  static thread_local VkResult backend_error_;
};

}  // namespace acv::ui
