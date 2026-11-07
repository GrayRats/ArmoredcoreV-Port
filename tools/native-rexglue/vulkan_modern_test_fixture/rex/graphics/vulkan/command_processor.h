#pragma once
#include <rex/ui/vulkan/device.h>
namespace rex::graphics::vulkan {
class VulkanCommandProcessor {public:
 struct TestDevice {rex::ui::vulkan::VulkanDevice::Functions table{};const auto& functions()const{return table;}} device;
 const TestDevice* GetVulkanDevice()const{return &device;}
};
}
