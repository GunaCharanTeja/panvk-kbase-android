// SPDX-License-Identifier: GPL-2.0-or-later
// Direct-ICD robust SSBO fragment correctness oracle.

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kWidth = 8;
constexpr uint32_t kHeight = 1;
constexpr VkDeviceSize kReadbackBytes = kWidth * kHeight * 4 * sizeof(uint32_t);
constexpr VkDeviceSize kDataBytes = 64 * sizeof(uint32_t);
constexpr uint64_t kFenceTimeoutNs = 5'000'000'000ull;

[[noreturn]] void fail(const std::string &message)
{
   throw std::runtime_error(message);
}

void check(VkResult result, const char *what)
{
   if (result != VK_SUCCESS)
      fail(std::string(what) + " VkResult=" +
           std::to_string(static_cast<int>(result)));
}

std::vector<uint32_t> read_spv(const char *path)
{
   std::ifstream file(path, std::ios::binary | std::ios::ate);
   if (!file)
      fail(std::string("cannot open SPIR-V ") + path);
   const std::streamsize size = file.tellg();
   if (size <= 0 || size % 4 != 0)
      fail(std::string("invalid SPIR-V size ") + path);
   file.seekg(0);
   std::vector<uint32_t> words(static_cast<size_t>(size) / 4);
   if (!file.read(reinterpret_cast<char *>(words.data()), size) ||
       words.front() != 0x07230203)
      fail(std::string("invalid SPIR-V ") + path);
   return words;
}

uint32_t memory_type(const VkPhysicalDeviceMemoryProperties &properties,
                     uint32_t type_bits, VkMemoryPropertyFlags required,
                     VkMemoryPropertyFlags preferred)
{
   for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
      const VkMemoryPropertyFlags flags = properties.memoryTypes[i].propertyFlags;
      if ((type_bits & (1u << i)) && (flags & required) == required &&
          (flags & preferred) == preferred)
         return i;
   }
   for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
      const VkMemoryPropertyFlags flags = properties.memoryTypes[i].propertyFlags;
      if ((type_bits & (1u << i)) && (flags & required) == required)
         return i;
   }
   fail("no compatible memory type");
}

struct DeviceFns {
   PFN_vkGetDeviceQueue getDeviceQueue{};
   PFN_vkCreateBuffer createBuffer{};
   PFN_vkGetBufferMemoryRequirements getBufferMemoryRequirements{};
   PFN_vkBindBufferMemory bindBufferMemory{};
   PFN_vkMapMemory mapMemory{};
   PFN_vkUnmapMemory unmapMemory{};
   PFN_vkFlushMappedMemoryRanges flushMappedMemoryRanges{};
   PFN_vkInvalidateMappedMemoryRanges invalidateMappedMemoryRanges{};
   PFN_vkCreateImage createImage{};
   PFN_vkGetImageMemoryRequirements getImageMemoryRequirements{};
   PFN_vkBindImageMemory bindImageMemory{};
   PFN_vkCreateImageView createImageView{};
   PFN_vkAllocateMemory allocateMemory{};
   PFN_vkCreateShaderModule createShaderModule{};
   PFN_vkCreateDescriptorSetLayout createDescriptorSetLayout{};
   PFN_vkCreateDescriptorPool createDescriptorPool{};
   PFN_vkAllocateDescriptorSets allocateDescriptorSets{};
   PFN_vkUpdateDescriptorSets updateDescriptorSets{};
   PFN_vkCreatePipelineLayout createPipelineLayout{};
   PFN_vkCreateGraphicsPipelines createGraphicsPipelines{};
   PFN_vkCreateCommandPool createCommandPool{};
   PFN_vkAllocateCommandBuffers allocateCommandBuffers{};
   PFN_vkResetCommandBuffer resetCommandBuffer{};
   PFN_vkBeginCommandBuffer beginCommandBuffer{};
   PFN_vkEndCommandBuffer endCommandBuffer{};
   PFN_vkCmdPipelineBarrier cmdPipelineBarrier{};
   PFN_vkCmdBeginRendering cmdBeginRendering{};
   PFN_vkCmdEndRendering cmdEndRendering{};
   PFN_vkCmdBindPipeline cmdBindPipeline{};
   PFN_vkCmdBindDescriptorSets cmdBindDescriptorSets{};
   PFN_vkCmdPushConstants cmdPushConstants{};
   PFN_vkCmdSetViewport cmdSetViewport{};
   PFN_vkCmdSetScissor cmdSetScissor{};
   PFN_vkCmdDraw cmdDraw{};
   PFN_vkCmdCopyImageToBuffer cmdCopyImageToBuffer{};
   PFN_vkCreateFence createFence{};
   PFN_vkResetFences resetFences{};
   PFN_vkQueueSubmit queueSubmit{};
   PFN_vkWaitForFences waitForFences{};
   PFN_vkDestroyFence destroyFence{};
   PFN_vkDestroyCommandPool destroyCommandPool{};
   PFN_vkDestroyPipeline destroyPipeline{};
   PFN_vkDestroyPipelineLayout destroyPipelineLayout{};
   PFN_vkDestroyShaderModule destroyShaderModule{};
   PFN_vkDestroyDescriptorPool destroyDescriptorPool{};
   PFN_vkDestroyDescriptorSetLayout destroyDescriptorSetLayout{};
   PFN_vkDestroyImageView destroyImageView{};
   PFN_vkDestroyImage destroyImage{};
   PFN_vkDestroyBuffer destroyBuffer{};
   PFN_vkFreeMemory freeMemory{};
   PFN_vkDestroyDevice destroyDevice{};
};

template <typename T>
T load_device(PFN_vkGetDeviceProcAddr get_proc_addr, VkDevice device,
              const char *name)
{
   auto function = reinterpret_cast<T>(get_proc_addr(device, name));
   if (!function)
      fail(std::string("missing ") + name);
   return function;
}

DeviceFns load_device_functions(PFN_vkGetDeviceProcAddr get_proc_addr,
                                VkDevice device)
{
   DeviceFns f;
#define LOAD(name, member) \
   f.member = load_device<PFN_vk##name>(get_proc_addr, device, "vk" #name)
   LOAD(GetDeviceQueue, getDeviceQueue);
   LOAD(CreateBuffer, createBuffer);
   LOAD(GetBufferMemoryRequirements, getBufferMemoryRequirements);
   LOAD(BindBufferMemory, bindBufferMemory);
   LOAD(MapMemory, mapMemory);
   LOAD(UnmapMemory, unmapMemory);
   LOAD(FlushMappedMemoryRanges, flushMappedMemoryRanges);
   LOAD(InvalidateMappedMemoryRanges, invalidateMappedMemoryRanges);
   LOAD(CreateImage, createImage);
   LOAD(GetImageMemoryRequirements, getImageMemoryRequirements);
   LOAD(BindImageMemory, bindImageMemory);
   LOAD(CreateImageView, createImageView);
   LOAD(AllocateMemory, allocateMemory);
   LOAD(CreateShaderModule, createShaderModule);
   LOAD(CreateDescriptorSetLayout, createDescriptorSetLayout);
   LOAD(CreateDescriptorPool, createDescriptorPool);
   LOAD(AllocateDescriptorSets, allocateDescriptorSets);
   LOAD(UpdateDescriptorSets, updateDescriptorSets);
   LOAD(CreatePipelineLayout, createPipelineLayout);
   LOAD(CreateGraphicsPipelines, createGraphicsPipelines);
   LOAD(CreateCommandPool, createCommandPool);
   LOAD(AllocateCommandBuffers, allocateCommandBuffers);
   LOAD(ResetCommandBuffer, resetCommandBuffer);
   LOAD(BeginCommandBuffer, beginCommandBuffer);
   LOAD(EndCommandBuffer, endCommandBuffer);
   LOAD(CmdPipelineBarrier, cmdPipelineBarrier);
   LOAD(CmdBeginRendering, cmdBeginRendering);
   LOAD(CmdEndRendering, cmdEndRendering);
   LOAD(CmdBindPipeline, cmdBindPipeline);
   LOAD(CmdBindDescriptorSets, cmdBindDescriptorSets);
   LOAD(CmdPushConstants, cmdPushConstants);
   LOAD(CmdSetViewport, cmdSetViewport);
   LOAD(CmdSetScissor, cmdSetScissor);
   LOAD(CmdDraw, cmdDraw);
   LOAD(CmdCopyImageToBuffer, cmdCopyImageToBuffer);
   LOAD(CreateFence, createFence);
   LOAD(ResetFences, resetFences);
   LOAD(QueueSubmit, queueSubmit);
   LOAD(WaitForFences, waitForFences);
   LOAD(DestroyFence, destroyFence);
   LOAD(DestroyCommandPool, destroyCommandPool);
   LOAD(DestroyPipeline, destroyPipeline);
   LOAD(DestroyPipelineLayout, destroyPipelineLayout);
   LOAD(DestroyShaderModule, destroyShaderModule);
   LOAD(DestroyDescriptorPool, destroyDescriptorPool);
   LOAD(DestroyDescriptorSetLayout, destroyDescriptorSetLayout);
   LOAD(DestroyImageView, destroyImageView);
   LOAD(DestroyImage, destroyImage);
   LOAD(DestroyBuffer, destroyBuffer);
   LOAD(FreeMemory, freeMemory);
   LOAD(DestroyDevice, destroyDevice);
#undef LOAD
   return f;
}

struct Buffer {
   VkBuffer buffer{};
   VkDeviceMemory memory{};
   VkDeviceSize allocation_size{};
   void *mapped{};
   bool coherent{};
};

struct Image {
   VkImage image{};
   VkDeviceMemory memory{};
   VkImageView view{};
};

void barrier_image(const DeviceFns &f, VkCommandBuffer command, VkImage image,
                   VkImageLayout old_layout, VkImageLayout new_layout,
                   VkAccessFlags src_access, VkAccessFlags dst_access,
                   VkPipelineStageFlags src_stage,
                   VkPipelineStageFlags dst_stage)
{
   VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
   barrier.oldLayout = old_layout;
   barrier.newLayout = new_layout;
   barrier.srcAccessMask = src_access;
   barrier.dstAccessMask = dst_access;
   barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.image = image;
   barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
   f.cmdPipelineBarrier(command, src_stage, dst_stage, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);
}

Buffer create_host_buffer(const DeviceFns &f,
                          const VkPhysicalDeviceMemoryProperties &memory,
                          VkDevice device, VkDeviceSize size,
                          VkBufferUsageFlags usage)
{
   Buffer result;
   VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
   info.size = size;
   info.usage = usage;
   info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
   check(f.createBuffer(device, &info, nullptr, &result.buffer), "create buffer");
   VkMemoryRequirements requirements{};
   f.getBufferMemoryRequirements(device, result.buffer, &requirements);
   VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
   allocate.allocationSize = requirements.size;
   allocate.memoryTypeIndex = memory_type(
      memory, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
   result.allocation_size = requirements.size;
   result.coherent = (memory.memoryTypes[allocate.memoryTypeIndex].propertyFlags &
                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
   check(f.allocateMemory(device, &allocate, nullptr, &result.memory),
         "allocate buffer memory");
   check(f.bindBufferMemory(device, result.buffer, result.memory, 0),
         "bind buffer memory");
   check(f.mapMemory(device, result.memory, 0, VK_WHOLE_SIZE, 0, &result.mapped),
         "map buffer memory");
   return result;
}

Image create_color_image(const DeviceFns &f,
                         const VkPhysicalDeviceMemoryProperties &memory,
                         VkDevice device, VkFormat format)
{
   Image result;
   VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
   info.imageType = VK_IMAGE_TYPE_2D;
   info.format = format;
   info.extent = {kWidth, kHeight, 1};
   info.mipLevels = 1;
   info.arrayLayers = 1;
   info.samples = VK_SAMPLE_COUNT_1_BIT;
   info.tiling = VK_IMAGE_TILING_OPTIMAL;
   info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
   info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
   check(f.createImage(device, &info, nullptr, &result.image), "create image");
   VkMemoryRequirements requirements{};
   f.getImageMemoryRequirements(device, result.image, &requirements);
   VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
   allocate.allocationSize = requirements.size;
   allocate.memoryTypeIndex = memory_type(
      memory, requirements.memoryTypeBits, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
   check(f.allocateMemory(device, &allocate, nullptr, &result.memory),
         "allocate image memory");
   check(f.bindImageMemory(device, result.image, result.memory, 0),
         "bind image memory");
   VkImageViewCreateInfo view_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
   view_info.image = result.image;
   view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
   view_info.format = format;
   view_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
   check(f.createImageView(device, &view_info, nullptr, &result.view),
         "create image view");
   return result;
}

VkShaderModule create_shader(const DeviceFns &f, VkDevice device,
                             const char *path)
{
   const std::vector<uint32_t> code = read_spv(path);
   VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
   info.codeSize = code.size() * sizeof(uint32_t);
   info.pCode = code.data();
   VkShaderModule shader{};
   check(f.createShaderModule(device, &info, nullptr, &shader),
         "create shader module");
   return shader;
}

VkPipeline create_pipeline(const DeviceFns &f, VkDevice device,
                           VkPipelineLayout layout, VkShaderModule vertex,
                           VkShaderModule fragment)
{
   VkPipelineShaderStageCreateInfo stages[2] = {
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
   };
   stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
   stages[0].module = vertex;
   stages[0].pName = "main";
   stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
   stages[1].module = fragment;
   stages[1].pName = "main";
   VkPipelineVertexInputStateCreateInfo vertex_input{
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
   VkPipelineInputAssemblyStateCreateInfo input_assembly{
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
   input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
   VkPipelineViewportStateCreateInfo viewport{
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
   viewport.viewportCount = 1;
   viewport.scissorCount = 1;
   VkPipelineRasterizationStateCreateInfo raster{
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
   raster.polygonMode = VK_POLYGON_MODE_FILL;
   raster.cullMode = VK_CULL_MODE_NONE;
   raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
   raster.lineWidth = 1.0f;
   VkPipelineMultisampleStateCreateInfo multisample{
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
   multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
   VkPipelineColorBlendAttachmentState blend_attachment{};
   blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                     VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT |
                                     VK_COLOR_COMPONENT_A_BIT;
   VkPipelineColorBlendStateCreateInfo blend{
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
   blend.attachmentCount = 1;
   blend.pAttachments = &blend_attachment;
   VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                      VK_DYNAMIC_STATE_SCISSOR};
   VkPipelineDynamicStateCreateInfo dynamic{
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
   dynamic.dynamicStateCount = 2;
   dynamic.pDynamicStates = dynamic_states;
   VkFormat format = VK_FORMAT_R32G32B32A32_UINT;
   VkPipelineRenderingCreateInfo rendering{
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
   rendering.colorAttachmentCount = 1;
   rendering.pColorAttachmentFormats = &format;
   VkGraphicsPipelineCreateInfo info{
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
   info.pNext = &rendering;
   info.stageCount = 2;
   info.pStages = stages;
   info.pVertexInputState = &vertex_input;
   info.pInputAssemblyState = &input_assembly;
   info.pViewportState = &viewport;
   info.pRasterizationState = &raster;
   info.pMultisampleState = &multisample;
   info.pColorBlendState = &blend;
   info.pDynamicState = &dynamic;
   info.layout = layout;
   VkPipeline pipeline{};
   check(f.createGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr,
                                   &pipeline),
         "create graphics pipeline");
   return pipeline;
}

struct Case {
   const char *name;
   uint32_t range0;
   uint32_t range1;
   uint32_t base_dword;
   std::array<uint32_t, 4> expected;
   uint32_t stride_dword = 0;
};

uint32_t value(unsigned buffer, unsigned index)
{
   return 0x10000000u + buffer * 0x00100000u + index * 0x101u;
}

uint32_t robust_value(unsigned buffer, uint32_t range, uint32_t dword)
{
   const uint32_t byte_offset = dword * 4u;
   if (range < 4 || byte_offset > range - 4)
      return 0;
   return value(buffer, dword);
}

std::array<uint32_t, 4> grouped_expected(uint32_t range, uint32_t base)
{
   std::array<uint32_t, 4> result{};
   for (unsigned lane = 0; lane < 4; ++lane)
      result[lane] = robust_value(0, range, base + lane) ^
                     robust_value(0, range, base + lane + 4);
   return result;
}

std::array<uint32_t, 4> separate_expected(uint32_t range0, uint32_t range1,
                                           uint32_t base)
{
   std::array<uint32_t, 4> result{};
   for (unsigned lane = 0; lane < 4; ++lane)
      result[lane] = robust_value(0, range0, base + lane * 2) ^
                     robust_value(1, range1, base + lane * 2 + 1);
   return result;
}

void expect_pixels(const Case &test, const uint32_t *actual, bool separate)
{
   for (unsigned pixel = 0; pixel < kWidth * kHeight; ++pixel) {
      const uint32_t base = test.base_dword + pixel * test.stride_dword;
      const std::array<uint32_t, 4> expected =
         separate ? separate_expected(test.range0, test.range1, base)
                  : grouped_expected(test.range0, base);
      for (unsigned lane = 0; lane < 4; ++lane) {
         if (actual[pixel * 4 + lane] != expected[lane]) {
            fail(std::string("mismatch case=") + test.name + " pixel=" +
                 std::to_string(pixel) + " lane=" + std::to_string(lane) +
                 " got=0x" + std::to_string(actual[pixel * 4 + lane]) +
                 " expected=0x" + std::to_string(expected[lane]));
         }
      }
   }
   std::printf("PASS case=%s range0=%u range1=%u base_dword=0x%08x output=%08x,%08x,%08x,%08x\n",
               test.name, test.range0, test.range1, test.base_dword,
               actual[0], actual[1], actual[2], actual[3]);
}

} // namespace

int main(int argc, char **argv)
{
   if (argc != 3 || argv[1][0] != '/') {
      std::fprintf(stderr, "usage: %s /path/to/libvulkan_panfrost.so build-dir\n", argv[0]);
      return 2;
   }
   try {
      void *library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
      if (!library)
         fail(dlerror());
      auto get_instance_proc_addr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
         dlsym(library, "vk_icdGetInstanceProcAddr"));
      auto negotiate = reinterpret_cast<VkResult (*)(uint32_t *)>(
         dlsym(library, "vk_icdNegotiateLoaderICDInterfaceVersion"));
      if (!get_instance_proc_addr || !negotiate)
         fail("direct ICD entry points missing");
      uint32_t interface_version = 7;
      check(negotiate(&interface_version), "ICD negotiation");
      auto create_instance = reinterpret_cast<PFN_vkCreateInstance>(
         get_instance_proc_addr(nullptr, "vkCreateInstance"));
      if (!create_instance)
         fail("missing vkCreateInstance");
      VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
      app.apiVersion = VK_API_VERSION_1_3;
      VkInstanceCreateInfo instance_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
      instance_info.pApplicationInfo = &app;
      VkInstance instance{};
      check(create_instance(&instance_info, nullptr, &instance), "create instance");
      auto enumerate_physical_devices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
         get_instance_proc_addr(instance, "vkEnumeratePhysicalDevices"));
      auto get_physical_device_properties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
         get_instance_proc_addr(instance, "vkGetPhysicalDeviceProperties"));
      auto get_physical_device_properties2 =
         reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
            get_instance_proc_addr(instance, "vkGetPhysicalDeviceProperties2"));
      auto get_physical_device_features2 = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
         get_instance_proc_addr(instance, "vkGetPhysicalDeviceFeatures2"));
      auto get_queue_family_properties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
         get_instance_proc_addr(instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
      auto get_memory_properties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
         get_instance_proc_addr(instance, "vkGetPhysicalDeviceMemoryProperties"));
      auto get_format_properties = reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(
         get_instance_proc_addr(instance, "vkGetPhysicalDeviceFormatProperties"));
      auto create_device = reinterpret_cast<PFN_vkCreateDevice>(
         get_instance_proc_addr(instance, "vkCreateDevice"));
      auto destroy_instance = reinterpret_cast<PFN_vkDestroyInstance>(
         get_instance_proc_addr(instance, "vkDestroyInstance"));
      auto get_device_proc_addr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
         get_instance_proc_addr(instance, "vkGetDeviceProcAddr"));
      if (!enumerate_physical_devices || !get_physical_device_properties ||
          !get_physical_device_properties2 || !get_physical_device_features2 ||
          !get_queue_family_properties || !get_memory_properties ||
          !get_format_properties || !create_device || !destroy_instance ||
          !get_device_proc_addr)
         fail("required instance entry point missing");
      uint32_t physical_count = 0;
      check(enumerate_physical_devices(instance, &physical_count, nullptr),
            "enumerate physical device count");
      if (physical_count != 1)
         fail("expected exactly one physical device");
      VkPhysicalDevice physical{};
      check(enumerate_physical_devices(instance, &physical_count, &physical),
            "enumerate physical device");
      VkPhysicalDeviceProperties properties{};
      get_physical_device_properties(physical, &properties);
      VkPhysicalDeviceRobustness2PropertiesEXT robust_properties{
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_PROPERTIES_EXT};
      VkPhysicalDeviceProperties2 properties2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
      properties2.pNext = &robust_properties;
      get_physical_device_properties2(physical, &properties2);
      const uint32_t size_alignment =
         robust_properties.robustStorageBufferAccessSizeAlignment;
      if (size_alignment != 1 && size_alignment != 4)
         fail("unexpected robustStorageBufferAccessSizeAlignment=" +
              std::to_string(size_alignment));
      std::printf("gpu=%s vendor=0x%04x device=0x%08x robust2_size_alignment=%u\n",
                  properties.deviceName, properties.vendorID, properties.deviceID,
                  size_alignment);
      VkPhysicalDeviceRobustness2FeaturesEXT robust_features{
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT};
      VkPhysicalDeviceDynamicRenderingFeatures dynamic{
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES};
      VkPhysicalDeviceFeatures2 features2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
      dynamic.pNext = &robust_features;
      features2.pNext = &dynamic;
      get_physical_device_features2(physical, &features2);
      if (!robust_features.robustBufferAccess2)
         fail("robustBufferAccess2 unsupported");
      if (!features2.features.robustBufferAccess)
         fail("core robustBufferAccess unsupported");
      if (!dynamic.dynamicRendering)
         fail("dynamicRendering unsupported");
      uint32_t family_count = 0;
      get_queue_family_properties(physical, &family_count, nullptr);
      std::vector<VkQueueFamilyProperties> families(family_count);
      get_queue_family_properties(physical, &family_count, families.data());
      uint32_t family = family_count;
      for (uint32_t i = 0; i < family_count; ++i) {
         if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && families[i].queueCount) {
            family = i;
            break;
         }
      }
      if (family == family_count)
         fail("graphics queue missing");
      float priority = 1.0f;
      VkDeviceQueueCreateInfo queue_info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
      queue_info.queueFamilyIndex = family;
      queue_info.queueCount = 1;
      queue_info.pQueuePriorities = &priority;
      VkPhysicalDeviceRobustness2FeaturesEXT enabled_robust{
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT};
      enabled_robust.robustBufferAccess2 = VK_TRUE;
      VkPhysicalDeviceDynamicRenderingFeatures enabled_dynamic{
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES};
      enabled_dynamic.dynamicRendering = VK_TRUE;
      enabled_dynamic.pNext = &enabled_robust;
      VkPhysicalDeviceFeatures2 enabled_features{
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
      enabled_features.features.robustBufferAccess = VK_TRUE;
      enabled_features.pNext = &enabled_dynamic;
      const char *extensions[] = {VK_EXT_ROBUSTNESS_2_EXTENSION_NAME};
      VkDeviceCreateInfo device_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
      device_info.pNext = &enabled_features;
      device_info.queueCreateInfoCount = 1;
      device_info.pQueueCreateInfos = &queue_info;
      device_info.enabledExtensionCount = 1;
      device_info.ppEnabledExtensionNames = extensions;
      VkDevice device{};
      check(create_device(physical, &device_info, nullptr, &device), "create device");
      DeviceFns f = load_device_functions(get_device_proc_addr, device);
      VkQueue queue{};
      f.getDeviceQueue(device, family, 0, &queue);
      VkPhysicalDeviceMemoryProperties memory{};
      get_memory_properties(physical, &memory);
      VkFormatProperties format_properties{};
      get_format_properties(physical, VK_FORMAT_R32G32B32A32_UINT,
                            &format_properties);
      const VkFormatFeatureFlags required_format =
         VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
      if ((format_properties.optimalTilingFeatures & required_format) != required_format)
         fail("R32G32B32A32_UINT color/transfer format unsupported");
      Buffer data = create_host_buffer(f, memory, device, kDataBytes,
                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
      Buffer other = create_host_buffer(f, memory, device, kDataBytes,
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
      for (unsigned i = 0; i < 16; ++i) {
         static_cast<uint32_t *>(data.mapped)[i] = value(0, i);
         static_cast<uint32_t *>(other.mapped)[i] = value(1, i);
      }
      if (!data.coherent) {
         VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
         range.memory = data.memory; range.size = VK_WHOLE_SIZE;
         check(f.flushMappedMemoryRanges(device, 1, &range), "flush data");
      }
      if (!other.coherent) {
         VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
         range.memory = other.memory; range.size = VK_WHOLE_SIZE;
         check(f.flushMappedMemoryRanges(device, 1, &range), "flush other");
      }
      Buffer readback = create_host_buffer(f, memory, device, kReadbackBytes,
                                           VK_BUFFER_USAGE_TRANSFER_DST_BIT);
      Image image = create_color_image(f, memory, device,
                                       VK_FORMAT_R32G32B32A32_UINT);
      VkDescriptorSetLayoutBinding bindings[2] = {};
      for (unsigned i = 0; i < 2; ++i) {
         bindings[i].binding = i;
         bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
         bindings[i].descriptorCount = 1;
         bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
      }
      VkDescriptorSetLayoutCreateInfo set_info{
         VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
      set_info.bindingCount = 2;
      set_info.pBindings = bindings;
      VkDescriptorSetLayout set_layout{};
      check(f.createDescriptorSetLayout(device, &set_info, nullptr, &set_layout),
            "create descriptor set layout");
      VkPushConstantRange push_range{VK_SHADER_STAGE_FRAGMENT_BIT, 0, 2 * sizeof(uint32_t)};
      VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
      layout_info.setLayoutCount = 1;
      layout_info.pSetLayouts = &set_layout;
      layout_info.pushConstantRangeCount = 1;
      layout_info.pPushConstantRanges = &push_range;
      VkPipelineLayout layout{};
      check(f.createPipelineLayout(device, &layout_info, nullptr, &layout),
            "create pipeline layout");
      VkDescriptorPoolSize pool_size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2};
      VkDescriptorPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
      pool_info.maxSets = 1;
      pool_info.poolSizeCount = 1;
      pool_info.pPoolSizes = &pool_size;
      VkDescriptorPool pool{};
      check(f.createDescriptorPool(device, &pool_info, nullptr, &pool),
            "create descriptor pool");
      VkDescriptorSetAllocateInfo set_allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
      set_allocate.descriptorPool = pool;
      set_allocate.descriptorSetCount = 1;
      set_allocate.pSetLayouts = &set_layout;
      VkDescriptorSet descriptor_set{};
      check(f.allocateDescriptorSets(device, &set_allocate, &descriptor_set),
            "allocate descriptor set");
      VkCommandPoolCreateInfo command_pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
      command_pool_info.queueFamilyIndex = family;
      command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
      VkCommandPool command_pool{};
      check(f.createCommandPool(device, &command_pool_info, nullptr, &command_pool),
            "create command pool");
      VkCommandBufferAllocateInfo command_allocate{
         VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
      command_allocate.commandPool = command_pool;
      command_allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      command_allocate.commandBufferCount = 1;
      VkCommandBuffer command{};
      check(f.allocateCommandBuffers(device, &command_allocate, &command),
            "allocate command buffer");
      VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
      VkFence fence{};
      check(f.createFence(device, &fence_info, nullptr, &fence), "create fence");
      const std::string build_dir = argv[2];
      const std::string vertex_path = build_dir + "/robust_ssbo.vert.spv";
      VkShaderModule vertex = create_shader(f, device, vertex_path.c_str());
      const std::array<Case, 11> grouped_cases = {{
         {"grouped-full", 32, 32, 0, grouped_expected(32, 0)},
         {"grouped-tail", 16, 16, 0, grouped_expected(16, 0)},
         {"grouped-short", 4, 4, 1, grouped_expected(4, 1)},
         {"grouped-partial-4", 4, 4, 0, grouped_expected(4, 0)},
         {"grouped-partial-8", 8, 8, 0, grouped_expected(8, 0)},
         {"grouped-partial-12", 12, 12, 0, grouped_expected(12, 0)},
         {"grouped-partial-20", 20, 20, 0, grouped_expected(20, 0)},
         {"grouped-partial-28", 28, 28, 0, grouped_expected(28, 0)},
         {"grouped-positive-oob", 4, 4, 16, grouped_expected(4, 16)},
         {"grouped-wrap", 4, 4, 0xffffffffu, grouped_expected(4, 0xffffffffu)},
         {"grouped-divergent", 16, 16, 0, grouped_expected(16, 0), 1},
      }};
      bool image_initialized = false;
      auto run_pipeline = [&](const char *fragment_name,
                              const std::vector<Case> &cases, bool separate) {
         const std::string path = build_dir + "/" + fragment_name;
         VkShaderModule fragment = create_shader(f, device, path.c_str());
         VkPipeline pipeline = create_pipeline(f, device, layout, vertex, fragment);
         for (const Case &test : cases) {
            VkDescriptorBufferInfo infos[2] = {{data.buffer, 0, test.range0},
                                                {other.buffer, 0, test.range1}};
            VkWriteDescriptorSet writes[2] = {
               {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
               {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
            };
            for (unsigned i = 0; i < 2; ++i) {
               writes[i].dstSet = descriptor_set;
               writes[i].dstBinding = i;
               writes[i].descriptorCount = 1;
               writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
               writes[i].pBufferInfo = &infos[i];
            }
            f.updateDescriptorSets(device, 2, writes, 0, nullptr);
            check(f.resetCommandBuffer(command, 0), "reset command buffer");
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            check(f.beginCommandBuffer(command, &begin), "begin command buffer");
            barrier_image(f, command, image.image,
                          image_initialized ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
                                             : VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          image_initialized ? VK_ACCESS_TRANSFER_READ_BIT : 0,
                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                          image_initialized ? VK_PIPELINE_STAGE_TRANSFER_BIT
                                             : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
            VkClearValue clear{};
            clear.color.uint32[0] = 0xdeadc0deu;
            clear.color.uint32[1] = 0xdeadc0deu;
            clear.color.uint32[2] = 0xdeadc0deu;
            clear.color.uint32[3] = 0xdeadc0deu;
            VkRenderingAttachmentInfo attachment{
               VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
            attachment.imageView = image.view;
            attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.clearValue = clear;
            VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};
            rendering.renderArea = {{0, 0}, {kWidth, kHeight}};
            rendering.layerCount = 1;
            rendering.colorAttachmentCount = 1;
            rendering.pColorAttachments = &attachment;
            f.cmdBeginRendering(command, &rendering);
            f.cmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            f.cmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0,
                                     1, &descriptor_set, 0, nullptr);
            const uint32_t push_constants[2] = {test.base_dword,
                                                test.stride_dword};
            f.cmdPushConstants(command, layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(push_constants), push_constants);
            VkViewport viewport{0, 0, float(kWidth), float(kHeight), 0, 1};
            VkRect2D scissor{{0, 0}, {kWidth, kHeight}};
            f.cmdSetViewport(command, 0, 1, &viewport);
            f.cmdSetScissor(command, 0, 1, &scissor);
            f.cmdDraw(command, 3, 1, 0, 0);
            f.cmdEndRendering(command);
            barrier_image(f, command, image.image,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                          VK_ACCESS_TRANSFER_READ_BIT,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT);
            VkBufferImageCopy copy{};
            copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.imageExtent = {kWidth, kHeight, 1};
            f.cmdCopyImageToBuffer(command, image.image,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.buffer,
                                   1, &copy);
            VkBufferMemoryBarrier buffer_barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            buffer_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            buffer_barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            buffer_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            buffer_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            buffer_barrier.buffer = readback.buffer;
            buffer_barrier.size = kReadbackBytes;
            f.cmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1,
                                  &buffer_barrier, 0, nullptr);
            check(f.endCommandBuffer(command), "end command buffer");
            check(f.resetFences(device, 1, &fence), "reset fence");
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &command;
            check(f.queueSubmit(queue, 1, &submit, fence), "queue submit");
            check(f.waitForFences(device, 1, &fence, VK_TRUE, kFenceTimeoutNs),
                  "wait for fence");
            if (!readback.coherent) {
               VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
               range.memory = readback.memory; range.size = VK_WHOLE_SIZE;
               check(f.invalidateMappedMemoryRanges(device, 1, &range),
                     "invalidate readback");
            }
            expect_pixels(test, static_cast<const uint32_t *>(readback.mapped), separate);
            image_initialized = true;
         }
         f.destroyPipeline(device, pipeline, nullptr);
         f.destroyShaderModule(device, fragment, nullptr);
      };
      run_pipeline("grouped.frag.spv", std::vector<Case>(grouped_cases.begin(), grouped_cases.end()), false);
      std::vector<Case> separate_cases = {
         {"separate-control", 32, 32, 0, separate_expected(32, 32, 0)},
      };
      run_pipeline("separate.frag.spv", separate_cases, true);
      std::printf("PASS robust-ssbo-driver-probe cases=12 pixels=%u\n", kWidth * kHeight);
      f.destroyFence(device, fence, nullptr);
      f.destroyCommandPool(device, command_pool, nullptr);
      f.destroyDescriptorPool(device, pool, nullptr);
      f.destroyDescriptorSetLayout(device, set_layout, nullptr);
      f.destroyPipelineLayout(device, layout, nullptr);
      f.destroyShaderModule(device, vertex, nullptr);
      f.destroyImageView(device, image.view, nullptr);
      f.destroyImage(device, image.image, nullptr);
      f.freeMemory(device, image.memory, nullptr);
      f.unmapMemory(device, data.memory);
      f.unmapMemory(device, other.memory);
      f.unmapMemory(device, readback.memory);
      f.destroyBuffer(device, data.buffer, nullptr);
      f.destroyBuffer(device, other.buffer, nullptr);
      f.destroyBuffer(device, readback.buffer, nullptr);
      f.freeMemory(device, data.memory, nullptr);
      f.freeMemory(device, other.memory, nullptr);
      f.freeMemory(device, readback.memory, nullptr);
      f.destroyDevice(device, nullptr);
      destroy_instance(instance, nullptr);
      dlclose(library);
      return 0;
   } catch (const std::exception &error) {
      std::fprintf(stderr, "FAIL %s\n", error.what());
      return 1;
   }
}
