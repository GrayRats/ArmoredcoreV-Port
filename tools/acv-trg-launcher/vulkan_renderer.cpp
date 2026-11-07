#include "vulkan_renderer.h"

#include <SDL_vulkan.h>
#include <imgui_impl_sdl2.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace acv::ui {
namespace {

constexpr uint32_t kLauncherVulkanApiVersion = VK_API_VERSION_1_4;

template <typename T, typename Enumerate>
std::vector<T> EnumerateVk(Enumerate enumerate, const char* operation) {
  uint32_t count = 0;
  VulkanRenderer::Check(enumerate(&count, nullptr), operation);
  for (unsigned attempt = 0; attempt != 4; ++attempt) {
    std::vector<T> values(count);
    VkResult result = enumerate(&count, values.empty() ? nullptr : values.data());
    if (result == VK_INCOMPLETE) {
      continue;
    }
    VulkanRenderer::Check(result, operation);
    values.resize(count);
    return values;
  }
  throw std::runtime_error(std::string(operation) + ": enumeration kept changing");
}

bool HasExtension(const std::vector<VkExtensionProperties>& extensions,
                  const char* name) {
  return std::any_of(extensions.begin(), extensions.end(), [name](const auto& ext) {
    return std::strcmp(ext.extensionName, name) == 0;
  });
}

}  // namespace

thread_local VkResult VulkanRenderer::backend_error_ = VK_SUCCESS;

void VulkanRenderer::Check(VkResult result, const char* operation) {
  if (result == VK_SUCCESS) return;
  std::string detail = operation;
  if (result == VK_ERROR_DEVICE_LOST) detail += " (Vulkan device lost)";
  if (result == VK_ERROR_SURFACE_LOST_KHR) detail += " (Vulkan surface lost)";
  detail += ": VkResult " + std::to_string(static_cast<int>(result));
  throw std::runtime_error(detail);
}

void VulkanRenderer::CheckBackendResult(VkResult result) {
  if (result != VK_SUCCESS && backend_error_ == VK_SUCCESS)
    backend_error_ = result;
}

void VulkanRenderer::CheckBackendStatus(const char* operation) {
  const VkResult result = backend_error_;
  backend_error_ = VK_SUCCESS;
  Check(result, operation);
}

VulkanRenderer::~VulkanRenderer() { Shutdown(); }

void VulkanRenderer::Initialize(SDL_Window* window) {
  if (!window) throw std::runtime_error("Vulkan renderer needs an SDL window");
  window_ = window;

  unsigned int extension_count = 0;
  if (SDL_Vulkan_GetInstanceExtensions(window_, &extension_count, nullptr) != SDL_TRUE)
    throw std::runtime_error(std::string("SDL Vulkan extensions: ") + SDL_GetError());
  std::vector<const char*> instance_extensions(extension_count);
  if (SDL_Vulkan_GetInstanceExtensions(window_, &extension_count,
                                       instance_extensions.data()) != SDL_TRUE)
    throw std::runtime_error(std::string("SDL Vulkan extensions: ") + SDL_GetError());
  instance_extensions.resize(extension_count);

  uint32_t loader_api_version = VK_API_VERSION_1_0;
  Check(vkEnumerateInstanceVersion(&loader_api_version), "vkEnumerateInstanceVersion");
  if (loader_api_version < kLauncherVulkanApiVersion) {
    throw std::runtime_error(
        "Vulkan API 1.4 is required; loader supports " +
        std::to_string(VK_VERSION_MAJOR(loader_api_version)) + "." +
        std::to_string(VK_VERSION_MINOR(loader_api_version)) + "." +
        std::to_string(VK_VERSION_PATCH(loader_api_version)));
  }

  auto available_instance_extensions = EnumerateVk<VkExtensionProperties>(
      [](uint32_t* count, VkExtensionProperties* values) {
        return vkEnumerateInstanceExtensionProperties(nullptr, count, values);
      },
      "vkEnumerateInstanceExtensionProperties");
  VkInstanceCreateFlags instance_flags = 0;
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
  if (HasExtension(available_instance_extensions,
                   VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
    const bool already_enabled = std::any_of(
        instance_extensions.begin(), instance_extensions.end(), [](const char* ext) {
          return std::strcmp(ext, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0;
        });
    if (!already_enabled)
      instance_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    instance_flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
  }
#endif
  for (const char* required : instance_extensions) {
    if (!HasExtension(available_instance_extensions, required))
      throw std::runtime_error(std::string("Vulkan instance extension unavailable: ") + required);
  }

  VkApplicationInfo app_info{};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Armored Core V Launcher";
  app_info.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
  app_info.pEngineName = "ACV Launcher";
  app_info.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
  app_info.apiVersion = kLauncherVulkanApiVersion;
  VkInstanceCreateInfo instance_info{};
  instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_info.flags = instance_flags;
  instance_info.pApplicationInfo = &app_info;
  instance_info.enabledExtensionCount = static_cast<uint32_t>(instance_extensions.size());
  instance_info.ppEnabledExtensionNames = instance_extensions.data();
  Check(vkCreateInstance(&instance_info, nullptr, &instance_), "vkCreateInstance");

  if (SDL_Vulkan_CreateSurface(window_, instance_, &surface_) != SDL_TRUE)
    throw std::runtime_error(std::string("SDL Vulkan surface: ") + SDL_GetError());
  SelectDevice();
  CreateDevice();

  VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 256},
      {VK_DESCRIPTOR_TYPE_SAMPLER, 256},
  };
  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 512;
  pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
  pool_info.pPoolSizes = pool_sizes;
  Check(vkCreateDescriptorPool(device_, &pool_info, nullptr, &descriptor_pool_),
        "vkCreateDescriptorPool");

  int width = 0, height = 0;
  SDL_Vulkan_GetDrawableSize(window_, &width, &height);
  if (width <= 0 || height <= 0)
    throw std::runtime_error("Vulkan launcher window has no drawable area");
  CreateWindowResources(width, height);

  if (!ImGui_ImplSDL2_InitForVulkan(window_))
    throw std::runtime_error("SDL ImGui Vulkan platform initialization failed");
  sdl_backend_ = true;

  ImGui_ImplVulkan_InitInfo init{};
  init.ApiVersion = kLauncherVulkanApiVersion;
  init.Instance = instance_;
  init.PhysicalDevice = physical_device_;
  init.Device = device_;
  init.QueueFamily = queue_family_;
  init.Queue = queue_;
  init.DescriptorPool = descriptor_pool_;
  init.MinImageCount = 2;
  init.ImageCount = window_data_.ImageCount;
  init.PipelineInfoMain.RenderPass = window_data_.RenderPass;
  init.PipelineInfoMain.Subpass = 0;
  init.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  init.CheckVkResultFn = CheckBackendResult;
  backend_error_ = VK_SUCCESS;
  if (!ImGui_ImplVulkan_Init(&init))
    throw std::runtime_error("Dear ImGui Vulkan renderer initialization failed");
  vulkan_backend_ = true;
  CheckBackendStatus("Dear ImGui Vulkan backend initialization");
}

void VulkanRenderer::SelectDevice() {
  auto devices = EnumerateVk<VkPhysicalDevice>(
      [this](uint32_t* count, VkPhysicalDevice* values) {
        return vkEnumeratePhysicalDevices(instance_, count, values);
      },
      "vkEnumeratePhysicalDevices");
  int best_score = -1;
  for (VkPhysicalDevice candidate : devices) {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(candidate, &properties);
    if (properties.apiVersion < kLauncherVulkanApiVersion) continue;

    auto extensions = EnumerateVk<VkExtensionProperties>(
        [candidate](uint32_t* count, VkExtensionProperties* values) {
          return vkEnumerateDeviceExtensionProperties(candidate, nullptr, count, values);
        },
        "vkEnumerateDeviceExtensionProperties");
    if (!HasExtension(extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) continue;

    uint32_t family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, nullptr);
    std::vector<VkQueueFamilyProperties> families(family_count);
    if (family_count)
      vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, families.data());
    uint32_t combined_family = UINT32_MAX;
    for (uint32_t i = 0; i < family_count; ++i) {
      if (!families[i].queueCount || !(families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
        continue;
      VkBool32 can_present = VK_FALSE;
      Check(vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface_, &can_present),
            "vkGetPhysicalDeviceSurfaceSupportKHR");
      if (can_present) {
        combined_family = i;
        break;
      }
    }
    if (combined_family == UINT32_MAX) continue;

    int score = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 3 :
                properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 2 : 1;
    if (score > best_score) {
      best_score = score;
      physical_device_ = candidate;
      queue_family_ = combined_family;
    }
  }
  if (physical_device_ == VK_NULL_HANDLE)
    throw std::runtime_error(
        "No Vulkan 1.4 device supports graphics, presentation and VK_KHR_swapchain");
}

void VulkanRenderer::CreateDevice() {
  auto extensions = EnumerateVk<VkExtensionProperties>(
      [this](uint32_t* count, VkExtensionProperties* values) {
        return vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, count, values);
      },
      "vkEnumerateDeviceExtensionProperties");
  std::vector<const char*> enabled_extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
  if (HasExtension(extensions, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
    enabled_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif
  const float priority = 1.0f;
  VkDeviceQueueCreateInfo queue_info{};
  queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_info.queueFamilyIndex = queue_family_;
  queue_info.queueCount = 1;
  queue_info.pQueuePriorities = &priority;
  VkPhysicalDeviceFeatures features{};
  VkDeviceCreateInfo device_info{};
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.queueCreateInfoCount = 1;
  device_info.pQueueCreateInfos = &queue_info;
  device_info.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size());
  device_info.ppEnabledExtensionNames = enabled_extensions.data();
  device_info.pEnabledFeatures = &features;
  Check(vkCreateDevice(physical_device_, &device_info, nullptr, &device_), "vkCreateDevice");
  vkGetDeviceQueue(device_, queue_family_, 0, &queue_);
  if (queue_ == VK_NULL_HANDLE) throw std::runtime_error("Vulkan graphics queue is null");
}

void VulkanRenderer::CreateWindowResources(int width, int height) {
  const auto formats = EnumerateVk<VkSurfaceFormatKHR>(
      [this](uint32_t* count, VkSurfaceFormatKHR* values) {
        return vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, count, values);
      },
      "vkGetPhysicalDeviceSurfaceFormatsKHR");
  if (formats.empty()) throw std::runtime_error("Vulkan surface exposes no image formats");
  constexpr VkFormat preferred[] = {
      VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB,
      VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM};
  auto chosen = formats.begin();
  for (VkFormat format : preferred) {
    auto found = std::find_if(formats.begin(), formats.end(), [format](const auto& item) {
      return item.format == format && item.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    });
    if (found != formats.end()) {
      chosen = found;
      break;
    }
  }
  if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
    window_data_.SurfaceFormat = {VK_FORMAT_B8G8R8A8_UNORM,
                                  VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
  else
    window_data_.SurfaceFormat = *chosen;

  const auto modes = EnumerateVk<VkPresentModeKHR>(
      [this](uint32_t* count, VkPresentModeKHR* values) {
        return vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, count,
                                                         values);
      },
      "vkGetPhysicalDeviceSurfacePresentModesKHR");
  if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_FIFO_KHR) == modes.end())
    throw std::runtime_error("Vulkan surface does not support FIFO presentation");
  window_data_.Surface = surface_;
  window_data_.PresentMode = VK_PRESENT_MODE_FIFO_KHR;
  window_data_.UseDynamicRendering = false;
  ImGui_ImplVulkanH_CreateOrResizeWindow(instance_, physical_device_, device_, &window_data_,
                                         queue_family_, nullptr, width, height, 2, 0);
  ValidateWindowResources();
}

void VulkanRenderer::ValidateWindowResources() const {
  if (window_data_.Swapchain == VK_NULL_HANDLE || window_data_.RenderPass == VK_NULL_HANDLE ||
      window_data_.ImageCount < 2 || window_data_.Frames.Size != int(window_data_.ImageCount) ||
      window_data_.FrameSemaphores.Size != int(window_data_.SemaphoreCount))
    throw std::runtime_error("Vulkan swapchain helper returned incomplete resources");
  for (uint32_t i = 0; i < window_data_.ImageCount; ++i) {
    const auto& frame = window_data_.Frames[int(i)];
    if (frame.Backbuffer == VK_NULL_HANDLE || frame.BackbufferView == VK_NULL_HANDLE ||
        frame.Framebuffer == VK_NULL_HANDLE || frame.CommandPool == VK_NULL_HANDLE ||
        frame.CommandBuffer == VK_NULL_HANDLE || frame.Fence == VK_NULL_HANDLE)
      throw std::runtime_error("Vulkan swapchain frame resources are incomplete");
  }
  for (uint32_t i = 0; i < window_data_.SemaphoreCount; ++i) {
    const auto& sem = window_data_.FrameSemaphores[int(i)];
    if (sem.ImageAcquiredSemaphore == VK_NULL_HANDLE ||
        sem.RenderCompleteSemaphore == VK_NULL_HANDLE)
      throw std::runtime_error("Vulkan swapchain semaphore resources are incomplete");
  }
}

void VulkanRenderer::NewFrame() {
  if (!vulkan_backend_) throw std::runtime_error("Vulkan renderer is not initialized");
  backend_error_ = VK_SUCCESS;
  ImGui_ImplVulkan_NewFrame();
  CheckBackendStatus("Dear ImGui Vulkan new frame");
}

void VulkanRenderer::RebuildSwapchain(int width, int height) {
  if (width <= 0 || height <= 0) return;
  Check(vkDeviceWaitIdle(device_), "vkDeviceWaitIdle before swapchain rebuild");
  backend_error_ = VK_SUCCESS;
  ImGui_ImplVulkan_SetMinImageCount(2);
  CheckBackendStatus("Dear ImGui swapchain synchronization");
  ImGui_ImplVulkanH_CreateOrResizeWindow(instance_, physical_device_, device_, &window_data_,
                                         queue_family_, nullptr, width, height, 2, 0);
  CheckBackendStatus("Dear ImGui swapchain rebuild");
  ValidateWindowResources();
  window_data_.FrameIndex = 0;
  window_data_.SemaphoreIndex = 0;
  swapchain_rebuild_ = false;
}

bool VulkanRenderer::Render(ImDrawData* draw_data) {
  if (!draw_data || !device_) throw std::runtime_error("Vulkan renderer received no draw data");
  int width = 0, height = 0;
  SDL_Vulkan_GetDrawableSize(window_, &width, &height);
  if (width <= 0 || height <= 0) return false;
  if (swapchain_rebuild_ || width != window_data_.Width || height != window_data_.Height)
    RebuildSwapchain(width, height);

  auto& semaphores = window_data_.FrameSemaphores[int(window_data_.SemaphoreIndex)];
  VkResult result = vkAcquireNextImageKHR(device_, window_data_.Swapchain, UINT64_MAX,
                                          semaphores.ImageAcquiredSemaphore, VK_NULL_HANDLE,
                                          &window_data_.FrameIndex);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    swapchain_rebuild_ = true;
    return false;
  }
  if (result == VK_SUBOPTIMAL_KHR)
    swapchain_rebuild_ = true;
  else
    Check(result, "vkAcquireNextImageKHR");

  auto& frame = window_data_.Frames[int(window_data_.FrameIndex)];
  Check(vkWaitForFences(device_, 1, &frame.Fence, VK_TRUE, UINT64_MAX), "vkWaitForFences");
  Check(vkResetFences(device_, 1, &frame.Fence), "vkResetFences");
  Check(vkResetCommandPool(device_, frame.CommandPool, 0), "vkResetCommandPool");
  VkCommandBufferBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  Check(vkBeginCommandBuffer(frame.CommandBuffer, &begin), "vkBeginCommandBuffer");

  VkRenderPassBeginInfo render{};
  render.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  render.renderPass = window_data_.RenderPass;
  render.framebuffer = frame.Framebuffer;
  render.renderArea.extent = {static_cast<uint32_t>(window_data_.Width),
                              static_cast<uint32_t>(window_data_.Height)};
  render.clearValueCount = 1;
  render.pClearValues = &window_data_.ClearValue;
  vkCmdBeginRenderPass(frame.CommandBuffer, &render, VK_SUBPASS_CONTENTS_INLINE);
  backend_error_ = VK_SUCCESS;
  ImGui_ImplVulkan_RenderDrawData(draw_data, frame.CommandBuffer);
  CheckBackendStatus("Dear ImGui Vulkan draw data");
  vkCmdEndRenderPass(frame.CommandBuffer);
  Check(vkEndCommandBuffer(frame.CommandBuffer), "vkEndCommandBuffer");

  const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submit{};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.waitSemaphoreCount = 1;
  submit.pWaitSemaphores = &semaphores.ImageAcquiredSemaphore;
  submit.pWaitDstStageMask = &wait_stage;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &frame.CommandBuffer;
  submit.signalSemaphoreCount = 1;
  submit.pSignalSemaphores = &semaphores.RenderCompleteSemaphore;
  Check(vkQueueSubmit(queue_, 1, &submit, frame.Fence), "vkQueueSubmit");

  VkPresentInfoKHR present{};
  present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present.waitSemaphoreCount = 1;
  present.pWaitSemaphores = &semaphores.RenderCompleteSemaphore;
  present.swapchainCount = 1;
  present.pSwapchains = &window_data_.Swapchain;
  present.pImageIndices = &window_data_.FrameIndex;
  result = vkQueuePresentKHR(queue_, &present);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    swapchain_rebuild_ = true;
  else
    Check(result, "vkQueuePresentKHR");
  window_data_.SemaphoreIndex =
      (window_data_.SemaphoreIndex + 1) % window_data_.SemaphoreCount;
  return true;
}

void VulkanRenderer::Shutdown() noexcept {
  if (device_) {
    const VkResult idle = vkDeviceWaitIdle(device_);
    if (idle != VK_SUCCESS)
      std::cerr << "Vulkan shutdown: vkDeviceWaitIdle returned " << int(idle) << '\n';
    if (vulkan_backend_) {
      ImGui_ImplVulkan_Shutdown();
      vulkan_backend_ = false;
    }
    if (sdl_backend_) {
      ImGui_ImplSDL2_Shutdown();
      sdl_backend_ = false;
    }
    if (window_data_.Swapchain || window_data_.RenderPass)
      ImGui_ImplVulkanH_DestroyWindow(instance_, device_, &window_data_, nullptr);
    if (surface_) vkDestroySurfaceKHR(instance_, surface_, nullptr);
    surface_ = VK_NULL_HANDLE;
    if (descriptor_pool_) vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
    descriptor_pool_ = VK_NULL_HANDLE;
    vkDestroyDevice(device_, nullptr);
    device_ = VK_NULL_HANDLE;
  } else if (instance_ && surface_) {
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
    surface_ = VK_NULL_HANDLE;
  }
  if (instance_) {
    vkDestroyInstance(instance_, nullptr);
    instance_ = VK_NULL_HANDLE;
  }
  window_ = nullptr;
}

}  // namespace acv::ui
