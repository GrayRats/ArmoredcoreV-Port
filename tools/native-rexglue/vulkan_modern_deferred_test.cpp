// Compile production decoder unchanged against a narrow device-table fixture.
#include <cassert>
#include <iostream>
#include <rex/graphics/vulkan/command_processor.h>
#include <rex/graphics/vulkan/deferred_command_buffer.h>
using namespace rex::graphics::vulkan;
unsigned legacy_calls = 0, scoped_calls = 0, raster_calls = 0;
VKAPI_ATTR void VKAPI_CALL Legacy(VkCommandBuffer, VkPipelineStageFlags src,
                                  VkPipelineStageFlags dst,
                                  VkDependencyFlags flags, uint32_t memories,
                                  const VkMemoryBarrier *, uint32_t n,
                                  const VkBufferMemoryBarrier *b,
                                  uint32_t images,
                                  const VkImageMemoryBarrier *image) {
  assert(src == VK_PIPELINE_STAGE_TRANSFER_BIT &&
         dst == VK_PIPELINE_STAGE_HOST_BIT && !flags && !memories && n == 2 &&
         images == 1);
  assert(b[0].offset == 64 && b[0].size == 128 && b[1].offset == 256 &&
         b[1].size == 512);
  assert(image[0].oldLayout == VK_IMAGE_LAYOUT_GENERAL &&
         image[0].newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  ++legacy_calls;
}
VKAPI_ATTR void VKAPI_CALL Scoped(VkCommandBuffer, const VkDependencyInfo *d) {
  assert(d->bufferMemoryBarrierCount == 2 && d->imageMemoryBarrierCount == 1 &&
         !d->memoryBarrierCount);
  auto &a = d->pBufferMemoryBarriers[0];
  auto &b = d->pBufferMemoryBarriers[1];
  assert(a.srcStageMask == VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT &&
         b.srcStageMask == VK_PIPELINE_STAGE_2_TRANSFER_BIT);
  assert(a.offset == 64 && b.offset == 256);
  assert(d->pImageMemoryBarriers[0].newLayout ==
         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  ++scoped_calls;
}
VKAPI_ATTR void VKAPI_CALL Cull(VkCommandBuffer, VkCullModeFlags mode) {
  assert(mode == VK_CULL_MODE_BACK_BIT);
  ++raster_calls;
}
VKAPI_ATTR void VKAPI_CALL Face(VkCommandBuffer, VkFrontFace face) {
  assert(face == VK_FRONT_FACE_CLOCKWISE);
  ++raster_calls;
}
int main() {
  VulkanCommandProcessor cp;
  cp.device.table.vkCmdPipelineBarrier = Legacy;
  DeferredCommandBuffer stream(cp, 8);
  struct Buffer : VkBufferMemoryBarrier {
    VkPipelineStageFlags src{}, dst{};
  };
  struct Image : VkImageMemoryBarrier {
    VkPipelineStageFlags src{}, dst{};
  };
  Buffer buffers[2]{};
  for (unsigned i = 0; i < 2; ++i) {
    buffers[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    buffers[i].offset = i ? 256 : 64;
    buffers[i].size = i ? 512 : 128;
    buffers[i].src = 0xDEAD;
  }
  Image image{};
  image.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  image.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
  image.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  stream.CmdVkLegacyPendingBarriers(VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    VK_PIPELINE_STAGE_HOST_BIT, 2, buffers, 1,
                                    &image);
  VkBufferMemoryBarrier2 scoped[2]{};
  for (unsigned i = 0; i < 2; ++i) {
    scoped[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    scoped[i].offset = buffers[i].offset;
    scoped[i].srcStageMask = i ? VK_PIPELINE_STAGE_2_TRANSFER_BIT
                               : VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  }
  VkImageMemoryBarrier2 scoped_image{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
  scoped_image.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  stream.CmdVkPipelineBarrier2(Scoped, 2, scoped, 1, &scoped_image);
  stream.CmdVkGuestRasterization(Cull, Face, VK_CULL_MODE_BACK_BIT,
                                 VK_FRONT_FACE_CLOCKWISE);
  // Mutate caller arrays: replay must consume owned serialized copies.
  buffers[0].offset = 0;
  scoped[0].offset = 0;
  scoped_image.newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  stream.Execute(VK_NULL_HANDLE);
  assert(legacy_calls == 1 && scoped_calls == 1 && raster_calls == 2);
  stream.Reset();
  stream.Execute(VK_NULL_HANDLE);
  assert(legacy_calls == 1 && scoped_calls == 1 && raster_calls == 2);
  std::cout << "PASS production deferred legacy flatten/sync2 "
               "stage+layout+deep copy, raster PFNs and reset\n";
}
