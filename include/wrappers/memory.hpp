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

#include "vulkan_include.hpp"
#include <vector>

namespace throttle::graphics {

class framebuffers final : public std::vector<vk::raii::Framebuffer> {
public:
  framebuffers(const vk::raii::Device &p_device, const std::vector<vk::raii::ImageView> &p_image_views,
               const vk::Extent2D &p_extent, const vk::raii::RenderPass &p_render_pass) {
    uint32_t n_framebuffers = p_image_views.size();
    this->reserve(n_framebuffers);
    for (uint32_t i = 0; i < n_framebuffers; i++) {
      vk::ImageView             attachments[] = {*p_image_views[i]};
      vk::FramebufferCreateInfo framebuffer_info{};
      framebuffer_info.renderPass = *p_render_pass;
      framebuffer_info.attachmentCount = 1;
      framebuffer_info.pAttachments = attachments;
      framebuffer_info.width = p_extent.width;
      framebuffer_info.height = p_extent.height;
      framebuffer_info.layers = 1;
      this->emplace_back(p_device, framebuffer_info);
    }
  }
};

uint32_t find_memory_type(const vk::PhysicalDeviceMemoryProperties &p_mem_properties, uint32_t &p_type_filter,
                          const vk::MemoryPropertyFlags &p_property_flags);

vk::raii::DeviceMemory allocate_device_memory(const vk::raii::Device                  &p_device,
                                              const vk::PhysicalDeviceMemoryProperties p_mem_properties,
                                              vk::MemoryRequirements                   p_mem_requirements,
                                              const vk::MemoryPropertyFlags            p_mem_property_flags);

template <typename T> std::size_t sizeof_vector(const std::vector<T> &vec) { return sizeof(T) * vec.size(); }

struct buffer final {
  vk::raii::Buffer       m_buffer{nullptr};
  vk::raii::DeviceMemory m_memory{nullptr};

  buffer(std::nullptr_t) {}

  buffer(const vk::raii::PhysicalDevice &p_phys_device, const vk::raii::Device &p_logical_device,
         const vk::DeviceSize p_size, const vk::BufferUsageFlags p_usage,
         vk::MemoryPropertyFlags p_property_flags = vk::MemoryPropertyFlagBits::eHostVisible |
                                                    vk::MemoryPropertyFlagBits::eHostCoherent)
      : m_buffer{p_logical_device.createBuffer(vk::BufferCreateInfo{.size = p_size, .usage = p_usage})},
        m_memory{allocate_device_memory(p_logical_device, p_phys_device.getMemoryProperties(),
                                        m_buffer.getMemoryRequirements(), p_property_flags)} {
    m_buffer.bindMemory(*m_memory, 0);
  }

  template <typename T>
  buffer(const vk::raii::PhysicalDevice &p_phys_device, const vk::raii::Device &p_logical_device,
         const vk::BufferUsageFlags p_usage, const std::vector<T> &p_data,
         vk::MemoryPropertyFlags p_property_flags = vk::MemoryPropertyFlagBits::eHostVisible |
                                                    vk::MemoryPropertyFlagBits::eHostCoherent)
      : m_buffer{p_logical_device.createBuffer(vk::BufferCreateInfo{.size = sizeof_vector(p_data), .usage = p_usage})},
        m_memory{allocate_device_memory(p_logical_device, p_phys_device.getMemoryProperties(),
                                        m_buffer.getMemoryRequirements(), p_property_flags)} {
    m_buffer.bindMemory(*m_memory, 0);
    vk::DeviceSize size = sizeof_vector(p_data);
    auto           memory = static_cast<char *>(m_memory.mapMemory(0, size));
    memcpy(memory, p_data.data(), size);
    m_memory.unmapMemory();
  }

  template <typename T> void update(const std::vector<T> &p_data) {
    vk::DeviceSize size = p_data.size();
    auto           memory = static_cast<char *>(m_memory.mapMemory(0, size));
    memcpy(memory, p_data.data(), size);
    m_memory.unmapMemory();
  }
};

class buffers final : public std::vector<buffer> {
public:
  buffers(const std::size_t p_size, const vk::raii::PhysicalDevice &p_phys_device,
          const vk::raii::Device &p_logical_device, const vk::BufferUsageFlags p_usage,
          vk::MemoryPropertyFlags p_property_flags = vk::MemoryPropertyFlagBits::eHostVisible |
                                                     vk::MemoryPropertyFlagBits::eHostCoherent) {
    std::vector<buffer>::reserve(p_size);
    for (unsigned i = 0; i < p_size; i++) {
      this->emplace_back(p_phys_device, p_logical_device, p_size, p_usage, p_property_flags);
    }
  }
};

} // namespace throttle::graphics