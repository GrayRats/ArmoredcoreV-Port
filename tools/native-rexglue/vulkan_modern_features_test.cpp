// Bounded same-input GPU oracle for the seven optional Vulkan paths.
#include "async_readback_transfer.h"
#include "persistent_pipeline_cache.h"
#include "readback_suballocation.h"
#include "timeline_submission.h"
#include "vertex_input_pipeline_libraries.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan.h>
using namespace acv;
static void Check(VkResult r) {
  if (r != VK_SUCCESS)
    throw std::runtime_error("VkResult=" + std::to_string(r));
}
static void Require(bool b, const char *s) {
  if (!b)
    throw std::runtime_error(s);
}
struct Context {
  VkInstance instance{};
  VkPhysicalDevice physical{};
  VkDevice device{};
  VkQueue queue{}, copyqueue{};
  uint32_t family = 0, queue_count = 0;
  VkPhysicalDeviceProperties props{};
  VkPhysicalDeviceMemoryProperties memory{};
  VkCommandPool commands{};
  VkCommandBuffer command{};
  VkBuffer source{};
  VkDeviceMemory source_mem{};
  bool gpl = false;
  uint64_t difference = 0;
  void Init() {
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.apiVersion = VK_API_VERSION_1_3;
    VkInstanceCreateInfo ii{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ii.pApplicationInfo = &app;
    std::vector<const char *> layers;
    uint32_t n = 0;
    Check(vkEnumerateInstanceLayerProperties(&n, nullptr));
    std::vector<VkLayerProperties> ls(n);
    Check(vkEnumerateInstanceLayerProperties(&n, ls.data()));
    for (auto &l : ls)
      if (!std::strcmp(l.layerName, "VK_LAYER_KHRONOS_validation"))
        layers.push_back("VK_LAYER_KHRONOS_validation");
    ii.enabledLayerCount = uint32_t(layers.size());
    ii.ppEnabledLayerNames = layers.data();
    Check(vkCreateInstance(&ii, nullptr, &instance));
    Check(vkEnumeratePhysicalDevices(instance, &n, nullptr));
    std::vector<VkPhysicalDevice> ds(n);
    Check(vkEnumeratePhysicalDevices(instance, &n, ds.data()));
    for (auto d : ds) {
      VkPhysicalDeviceProperties p;
      vkGetPhysicalDeviceProperties(d, &p);
      if (p.apiVersion >= VK_API_VERSION_1_3 &&
          p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        physical = d;
        props = p;
        break;
      }
    }
    Require(physical, "discrete Vulkan1.3 device required");
    vkGetPhysicalDeviceMemoryProperties(physical, &memory);
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &n, nullptr);
    std::vector<VkQueueFamilyProperties> fs(n);
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &n, fs.data());
    while (family < n && !(fs[family].queueFlags & VK_QUEUE_GRAPHICS_BIT))
      ++family;
    Require(family < n, "graphics queue");
    queue_count = std::min(2u, fs[family].queueCount);
    Check(vkEnumerateDeviceExtensionProperties(physical, nullptr, &n, nullptr));
    std::vector<VkExtensionProperties> ex(n);
    Check(
        vkEnumerateDeviceExtensionProperties(physical, nullptr, &n, ex.data()));
    bool a = false, b = false;
    for (auto &e : ex) {
      a |= !std::strcmp(e.extensionName,
                        VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME);
      b |=
          !std::strcmp(e.extensionName, VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
    }
    VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT gf{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT};
    VkPhysicalDeviceVulkan13Features f13{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    VkPhysicalDeviceVulkan12Features f12{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceFeatures2 features{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    features.pNext = &f12;
    f12.pNext = &f13;
    if (a && b)
      f13.pNext = &gf;
    vkGetPhysicalDeviceFeatures2(physical, &features);
    Require(f12.timelineSemaphore && f13.synchronization2,
            "timeline and sync2 required");
    gpl = a && b && gf.graphicsPipelineLibrary;
    f12 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    f12.timelineSemaphore = VK_TRUE;
    f12.pNext = &f13;
    f13 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    f13.synchronization2 = VK_TRUE;
    gf = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT};
    gf.graphicsPipelineLibrary = VK_TRUE;
    if (gpl)
      f13.pNext = &gf;
    const char *ext[] = {VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME,
                         VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME};
    float priorities[] = {1, 1};
    VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qi.queueFamilyIndex = family;
    qi.queueCount = queue_count;
    qi.pQueuePriorities = priorities;
    VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    di.pNext = &f12;
    di.queueCreateInfoCount = 1;
    di.pQueueCreateInfos = &qi;
    di.enabledExtensionCount = gpl ? 2 : 0;
    di.ppEnabledExtensionNames = ext;
    Check(vkCreateDevice(physical, &di, nullptr, &device));
    vkGetDeviceQueue(device, family, 0, &queue);
    vkGetDeviceQueue(device, family, queue_count - 1, &copyqueue);
    VkPhysicalDeviceTimelineSemaphoreProperties tl{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES};
    VkPhysicalDeviceProperties2 pp{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    pp.pNext = &tl;
    vkGetPhysicalDeviceProperties2(physical, &pp);
    difference = tl.maxTimelineSemaphoreValueDifference;
    VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pi.queueFamilyIndex = family;
    pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    Check(vkCreateCommandPool(device, &pi, nullptr, &commands));
    VkCommandBufferAllocateInfo ai{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = commands;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    Check(vkAllocateCommandBuffers(device, &ai, &command));
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = 1 << 20;
    bi.usage =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    Check(vkCreateBuffer(device, &bi, nullptr, &source));
    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(device, source, &req);
    VkMemoryAllocateInfo mi{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mi.allocationSize = req.size;
    mi.memoryTypeIndex =
        Type(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    Check(vkAllocateMemory(device, &mi, nullptr, &source_mem));
    Check(vkBindBufferMemory(device, source, source_mem, 0));
    std::cout << "device=" << props.deviceName
              << " driver=" << props.driverVersion << " queues=" << queue_count
              << " GPL=" << gpl << " validation=" << layers.size() << std::endl;
  }
  uint32_t Type(uint32_t bits, VkMemoryPropertyFlags flags) {
    for (uint32_t i = 0; i < memory.memoryTypeCount; ++i)
      if ((bits & (1u << i)) &&
          (memory.memoryTypes[i].propertyFlags & flags) == flags)
        return i;
    throw std::runtime_error("memory type");
  }
  void Begin() {
    Check(vkResetCommandBuffer(command, 0));
    VkCommandBufferBeginInfo b{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    b.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    Check(vkBeginCommandBuffer(command, &b));
  }
  void SubmitWait() {
    Check(vkEndCommandBuffer(command));
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &command;
    Check(vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE));
    Check(vkQueueWaitIdle(queue));
  }
  ~Context() {
    if (device) {
      Check(vkDeviceWaitIdle(device));
      vkDestroyBuffer(device, source, nullptr);
      vkFreeMemory(device, source_mem, nullptr);
      vkDestroyCommandPool(device, commands, nullptr);
      vkDestroyDevice(device, nullptr);
    }
    if (instance)
      vkDestroyInstance(instance, nullptr);
  }
};
static std::vector<uint32_t> Shader(const char *path) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  Require(bool(f), "shader file");
  size_t n = f.tellg();
  std::vector<uint32_t> b(n / 4);
  f.seekg(0);
  f.read((char *)b.data(), n);
  Require(bool(f), "shader read");
  return b;
}
static void Readback(Context &c) {
  ReadbackSuballocation pool(c.device, vkGetDeviceProcAddr, c.memory,
                             c.props.limits.nonCoherentAtomSize);
  Require(pool.ready(), "pool init");
  TimelineSubmission graphics(c.device, vkGetDeviceProcAddr, 0, c.difference);
  Check(graphics.initialization);
  AsyncReadbackTransfer transfer(c.device, vkGetDeviceProcAddr, c.family,
                                 c.difference);
  Require(transfer.ready(), "transfer init");
  uint64_t signal = 0;
  constexpr uint32_t bytes = 65536;
  VkDeviceSize offset = 0, span = 0, guard_offset = 0, guard_span = 0;
  Require(pool.Acquire(99, 256, guard_offset, guard_span), "guard acquire");
  Require(pool.Acquire(1, bytes, offset, span) && offset >= guard_span,
          "slice isolation");
  std::memset(pool.data(guard_offset), 0xA5, guard_span);
  VkMappedMemoryRange guard{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
  guard.memory = pool.memory();
  guard.offset = guard_offset;
  guard.size = guard_span;
  Check(vkFlushMappedMemoryRanges(c.device, 1, &guard));
  for (unsigned i = 0; i < 80; ++i) {
    c.Begin();
    vkCmdFillBuffer(c.command, c.source, 0, bytes, 0x10203040 + i);
    // Legacy and sync2 copies have identical data and visibility contracts.
    if (i % 2) {
      VkBufferMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
      barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
      barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
      barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex =
          VK_QUEUE_FAMILY_IGNORED;
      barrier.buffer = c.source;
      barrier.size = bytes;
      VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
      dependency.bufferMemoryBarrierCount = 1;
      dependency.pBufferMemoryBarriers = &barrier;
      vkCmdPipelineBarrier2(c.command, &dependency);
    } else {
      VkBufferMemoryBarrier b{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
      b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      b.buffer = c.source;
      b.size = bytes;
      vkCmdPipelineBarrier(c.command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &b,
                           0, nullptr);
    }
    Check(vkEndCommandBuffer(c.command));
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &c.command;
    uint64_t wait = transfer.latest();
    VkSemaphore semaphore = transfer.handle();
    VkPipelineStageFlags stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    if (wait) {
      si.waitSemaphoreCount = 1;
      si.pWaitSemaphores = &semaphore;
      si.pWaitDstStageMask = &stage;
    }
    ++signal;
    Check(graphics.Submit(c.queue, si, signal, signal - 1,
                          wait ? &wait : nullptr));
    VkBufferCopy copy{0, offset, bytes};
    Check(transfer.Submit(c.copyqueue, graphics.handle(), signal, c.source,
                          pool.buffer(), copy));
    Check(transfer.WaitForTicket(signal));
    Require(transfer.PublishedReady(signal),
            "publication before copy completion");
    Check(graphics.Wait(signal, 5'000'000'000ULL));
    VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
    range.memory = pool.memory();
    range.offset = offset;
    range.size = span;
    Check(vkInvalidateMappedMemoryRanges(c.device, 1, &range));
    auto *words = (const uint32_t *)pool.data(offset);
    for (unsigned j = 0; j < bytes / 4; ++j)
      Require(words[j] == 0x10203040 + i, "readback generation mismatch");
  }
  for (size_t i = 0; i < guard_span; ++i)
    Require(((uint8_t *)pool.data(guard_offset))[i] == 0xA5,
            "neighbor slice overwritten");
  pool.Release(1);
  VkDeviceSize reused = 0, reused_span = 0;
  Require(pool.Acquire(2, bytes, reused, reused_span) && reused == offset,
          "slice reuse");
  Check(vkDeviceWaitIdle(c.device));
  transfer.DestroyAfterDeviceStopped();
  graphics.DestroyAfterDeviceStopped();
  pool.DestroyAfterDeviceStopped();
  std::cout << "timeline/sync2/suballocation/transfer exact bytes and neighbor "
               "guards PASS; generations=80; actual second queue="
            << (c.queue_count > 1) << std::endl;
}
static void Raster(Context &c, const char *vertex, const char *fragment,
                   const char *cache_path) {
  VkPipelineLayout layout{};
  VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  Check(vkCreatePipelineLayout(c.device, &li, nullptr, &layout));
  VkShaderModule modules[2]{};
  const char *paths[] = {vertex, fragment};
  for (unsigned i = 0; i < 2; ++i) {
    auto code = Shader(paths[i]);
    VkShaderModuleCreateInfo mi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    mi.codeSize = code.size() * 4;
    mi.pCode = code.data();
    Check(vkCreateShaderModule(c.device, &mi, nullptr, &modules[i]));
  }
  VkAttachmentDescription attachment{};
  attachment.format = VK_FORMAT_R8G8B8A8_UNORM;
  attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkSubpassDescription sub{};
  sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  sub.colorAttachmentCount = 1;
  sub.pColorAttachments = &ref;
  VkSubpassDependency dependency{};
  dependency.srcSubpass = 0;
  dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
  dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependency.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  VkRenderPassCreateInfo ri{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
  ri.attachmentCount = 1;
  ri.pAttachments = &attachment;
  ri.subpassCount = 1;
  ri.pSubpasses = &sub;
  ri.dependencyCount = 1;
  ri.pDependencies = &dependency;
  VkRenderPass render{};
  Check(vkCreateRenderPass(c.device, &ri, nullptr, &render));
  VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  ii.imageType = VK_IMAGE_TYPE_2D;
  ii.format = attachment.format;
  ii.extent = {16, 16, 1};
  ii.mipLevels = ii.arrayLayers = 1;
  ii.samples = VK_SAMPLE_COUNT_1_BIT;
  ii.tiling = VK_IMAGE_TILING_OPTIMAL;
  ii.usage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  VkImage image{};
  Check(vkCreateImage(c.device, &ii, nullptr, &image));
  VkMemoryRequirements req{};
  vkGetImageMemoryRequirements(c.device, image, &req);
  VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  ai.allocationSize = req.size;
  ai.memoryTypeIndex =
      c.Type(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VkDeviceMemory memory{};
  Check(vkAllocateMemory(c.device, &ai, nullptr, &memory));
  Check(vkBindImageMemory(c.device, image, memory, 0));
  VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  vi.image = image;
  vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
  vi.format = attachment.format;
  vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  VkImageView view{};
  Check(vkCreateImageView(c.device, &vi, nullptr, &view));
  VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
  fi.renderPass = render;
  fi.attachmentCount = 1;
  fi.pAttachments = &view;
  fi.width = fi.height = 16;
  fi.layers = 1;
  VkFramebuffer frame{};
  Check(vkCreateFramebuffer(c.device, &fi, nullptr, &frame));
  ReadbackSuballocation pool(c.device, vkGetDeviceProcAddr, c.memory,
                             c.props.limits.nonCoherentAtomSize);
  Require(pool.ready(), "raster readback pool");
  VkDeviceSize offset = 0, span = 0;
  Require(pool.Acquire(1, 1024, offset, span), "raster slice");
  std::vector<uint8_t> reference;
  {
    PersistentPipelineCache cache(c.device, c.props, vkGetDeviceProcAddr,
                                  cache_path);
    Require(cache.handle(), "cache create");
    VertexInputPipelineLibraries libraries(c.device, vkCreateGraphicsPipelines,
                                           vkDestroyPipeline);
    for (unsigned mode = 0; mode < 3; ++mode) {
      if (mode == 2 && !c.gpl)
        continue;
      VkPipelineShaderStageCreateInfo stages[2]{};
      for (unsigned i = 0; i < 2; ++i) {
        stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[i].stage =
            i ? VK_SHADER_STAGE_FRAGMENT_BIT : VK_SHADER_STAGE_VERTEX_BIT;
        stages[i].module = modules[i];
        stages[i].pName = "main";
      }
      VkPipelineVertexInputStateCreateInfo input{
          VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
      VkPipelineInputAssemblyStateCreateInfo assembly{
          VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
      assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
      VkViewport viewport{0, 0, 16, 16, 0, 1};
      VkRect2D scissor{{0, 0}, {16, 16}};
      VkPipelineViewportStateCreateInfo vp{
          VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
      vp.viewportCount = vp.scissorCount = 1;
      vp.pViewports = &viewport;
      vp.pScissors = &scissor;
      VkPipelineRasterizationStateCreateInfo raster{
          VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
      raster.polygonMode = VK_POLYGON_MODE_FILL;
      raster.lineWidth = 1;
      raster.cullMode = VK_CULL_MODE_NONE;
      raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
      VkPipelineMultisampleStateCreateInfo ms{
          VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
      ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
      VkPipelineColorBlendAttachmentState color{};
      color.colorWriteMask = 15;
      VkPipelineColorBlendStateCreateInfo blend{
          VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
      blend.attachmentCount = 1;
      blend.pAttachments = &color;
      VkDynamicState dynamic[] = {VK_DYNAMIC_STATE_CULL_MODE,
                                  VK_DYNAMIC_STATE_FRONT_FACE};
      VkPipelineDynamicStateCreateInfo dy{
          VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
      dy.dynamicStateCount = 2;
      dy.pDynamicStates = dynamic;
      VkGraphicsPipelineCreateInfo pi{
          VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
      pi.stageCount = 2;
      pi.pStages = stages;
      pi.pVertexInputState = &input;
      pi.pInputAssemblyState = &assembly;
      pi.pViewportState = &vp;
      pi.pRasterizationState = &raster;
      pi.pMultisampleState = &ms;
      pi.pColorBlendState = &blend;
      pi.layout = layout;
      pi.renderPass = render;
      pi.basePipelineIndex = -1;
      if (mode)
        pi.pDynamicState = &dy;
      VkPipeline pipeline{};
      VkResult result = VK_SUCCESS;
      if (mode == 2) {
        Require(libraries.TryCreate(pi, cache.handle(), pipeline, result),
                "library path fallback");
        Check(result);
      } else
        Check(vkCreateGraphicsPipelines(c.device, cache.handle(), 1, &pi,
                                        nullptr, &pipeline));
      cache.Created();
      c.Begin();
      VkClearValue clear{};
      VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
      begin.renderPass = render;
      begin.framebuffer = frame;
      begin.renderArea = scissor;
      begin.clearValueCount = 1;
      begin.pClearValues = &clear;
      vkCmdBeginRenderPass(c.command, &begin, VK_SUBPASS_CONTENTS_INLINE);
      vkCmdBindPipeline(c.command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
      if (mode) {
        vkCmdSetCullMode(c.command, VK_CULL_MODE_NONE);
        vkCmdSetFrontFace(c.command, VK_FRONT_FACE_COUNTER_CLOCKWISE);
      }
      vkCmdDraw(c.command, 3, 1, 0, 0);
      vkCmdEndRenderPass(c.command);
      VkBufferImageCopy copy{};
      copy.bufferOffset = offset;
      copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
      copy.imageExtent = {16, 16, 1};
      vkCmdCopyImageToBuffer(c.command, image,
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             pool.buffer(), 1, &copy);
      VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
      host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
      host.srcQueueFamilyIndex = host.dstQueueFamilyIndex =
          VK_QUEUE_FAMILY_IGNORED;
      host.buffer = pool.buffer();
      host.offset = offset;
      host.size = 1024;
      vkCmdPipelineBarrier(c.command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &host,
                           0, nullptr);
      c.SubmitWait();
      VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
      range.memory = pool.memory();
      range.offset = offset;
      range.size = span;
      Check(vkInvalidateMappedMemoryRanges(c.device, 1, &range));
      auto *bytes = (uint8_t *)pool.data(offset);
      std::vector<uint8_t> actual(bytes, bytes + 1024);
      if (!mode) {
        reference = actual;
        Require(actual[0] > 0 && actual[3] == 255, "empty reference raster");
      } else
        Require(actual == reference, "static/dynamic/library image mismatch");
      vkDestroyPipeline(c.device, pipeline, nullptr);
    }
  }
  {
    PersistentPipelineCache reload(c.device, c.props, vkGetDeviceProcAddr,
                                   cache_path);
    Require(reload.loaded, "driver cache reload failed");
  }
  {
    std::ofstream f(cache_path, std::ios::binary | std::ios::trunc);
    f << "invalid";
  }
  {
    PersistentPipelineCache corrupt(c.device, c.props, vkGetDeviceProcAddr,
                                    cache_path);
    Require(corrupt.handle() && !corrupt.loaded, "corrupt cache not discarded");
  }
  pool.DestroyAfterDeviceStopped();
  vkDestroyFramebuffer(c.device, frame, nullptr);
  vkDestroyImageView(c.device, view, nullptr);
  vkDestroyImage(c.device, image, nullptr);
  vkFreeMemory(c.device, memory, nullptr);
  vkDestroyRenderPass(c.device, render, nullptr);
  for (auto m : modules)
    vkDestroyShaderModule(c.device, m, nullptr);
  vkDestroyPipelineLayout(c.device, layout, nullptr);
  std::cout
      << "static/dynamic/GPL pixels equal, persistent reload/corruption PASS"
      << std::endl;
}
int main(int argc, char **argv) {
  try {
    Require(argc == 4, "usage: vertex.spv fragment.spv cache-file");
    Context c;
    c.Init();
    Readback(c);
    Raster(c, argv[1], argv[2], argv[3]);
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
}
