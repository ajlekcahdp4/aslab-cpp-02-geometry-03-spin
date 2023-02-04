/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tsimmerman.ss@phystech.edu>, <alex.rom23@mail.ru> wrote this file.  As long as you
 * retain this notice you can do whatever you want with this stuff. If we meet
 * some day, and you think this stuff is worth it, you can buy me a beer in
 * return.
 * ----------------------------------------------------------------------------
 */

#pragma once

#include "vulkan_hpp_include.hpp"

#include "image.hpp"
#include "memory.hpp"
#include "utils.hpp"
namespace ezvk {

static inline vk::Format find_depth_format(const vk::raii::PhysicalDevice &p_device) {
  auto predicate = [&p_device](const auto &candidate) -> bool {
    auto props = p_device.getFormatProperties(candidate);
    if ((props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) ==
        vk::FormatFeatureFlagBits::eDepthStencilAttachment)
      return true;
    return false;
  };
  std::vector<vk::Format> candidates = {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint,
                                        vk::Format::eD24UnormS8Uint};

  return utils::find_all_that_satisfy(candidates.begin(), candidates.end(), predicate).front();
}

struct depth_buffer final {
public:
  image      m_image;
  image_view m_image_view;

  depth_buffer() = default;

  depth_buffer(const vk::raii::PhysicalDevice &p_device, const vk::raii::Device &l_device,
               const vk::Extent2D &extent2d) {
    auto depth_format = find_depth_format(p_device);
    m_image = {p_device,
               l_device,
               vk::Extent3D{.width = extent2d.width, .height = extent2d.height, .depth = 1},
               depth_format,
               vk::ImageTiling::eOptimal,
               vk::ImageUsageFlagBits::eDepthStencilAttachment,
               vk::MemoryPropertyFlagBits::eDeviceLocal};

    m_image_view = {l_device, m_image(), depth_format, vk::ImageAspectFlagBits::eDepth};
  }
};

} // namespace ezvk