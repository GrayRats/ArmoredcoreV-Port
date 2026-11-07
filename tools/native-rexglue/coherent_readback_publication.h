#pragma once
// Include after the embedding SDK's Vulkan API/platform configuration.
#include <vulkan/vulkan_core.h>
namespace acv {
// Completion and a device->HOST_READ barrier remain mandatory even for coherent
// memory. Flags must be the actual allocated memory type, never its purpose.
// Unknown flags take the unconditional-invalidation path. No wait happens here.
template<class Invalidate,class Publish>
bool PublishCompletedReadback(bool completed,bool skip_coherent,bool flags_known,
    VkMemoryPropertyFlags flags,Invalidate&& invalidate,Publish&& publish) noexcept {
  if(!completed) return false;
  constexpr VkMemoryPropertyFlags coherent_host=
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  const bool skip=skip_coherent && flags_known && (flags&coherent_host)==coherent_host;
  try {
    if(!skip && invalidate()!=VK_SUCCESS) return false;
    return publish();
  } catch(...) {return false;}
}
} // namespace acv
