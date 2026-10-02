/* Native offscreen clear -> sampled draw. No Wine or DXVK. */
IF(vkCreateImage); IF(vkGetImageMemoryRequirements); IF(vkBindImageMemory);
IF(vkCreateSampler); IF(vkCreateDescriptorSetLayout); IF(vkCreateDescriptorPool);
IF(vkAllocateDescriptorSets); IF(vkUpdateDescriptorSets); IF(vkCreatePipelineLayout);
IF(vkCreateShaderModule); IF(vkCreateGraphicsPipelines); IF(vkCmdBindPipeline);
IF(vkCmdBindDescriptorSets); IF(vkCmdDraw);

static VkImage sample_image;
static VkFramebuffer sample_fb;
static VkRenderPass sample_pass;
static VkPipeline sample_pipeline;
static VkPipelineLayout sample_layout;
static VkDescriptorSet sample_set;

static VkShaderModule sample_shader(const char *path)
{
   FILE *f = fopen(path, "rb");
   long size;
   uint32_t *code;
   VkShaderModule shader;
   if (!f || fseek(f, 0, SEEK_END)) FAIL("shader %s", path);
   size = ftell(f);
   if (size <= 0 || size % 4 || fseek(f, 0, SEEK_SET)) FAIL("shader size");
   code = malloc((size_t)size);
   if (!code || fread(code, 1, (size_t)size, f) != (size_t)size) FAIL("shader read");
   fclose(f);
   VkShaderModuleCreateInfo ci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = (size_t)size, .pCode = code };
   CK(vkCreateShaderModule(dev, &ci, NULL, &shader));
   free(code);
   return shader;
}

static void sample_init(VkPhysicalDevice pd, VkFormat fmt, VkRenderPass output_pass)
{
   LI(vkCreateImage); LI(vkGetImageMemoryRequirements); LI(vkBindImageMemory);
   LI(vkCreateSampler); LI(vkCreateDescriptorSetLayout); LI(vkCreateDescriptorPool);
   LI(vkAllocateDescriptorSets); LI(vkUpdateDescriptorSets); LI(vkCreatePipelineLayout);
   LI(vkCreateShaderModule); LI(vkCreateGraphicsPipelines); LI(vkCmdBindPipeline);
   LI(vkCmdBindDescriptorSets); LI(vkCmdDraw);
   VkImageCreateInfo ici = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = VK_IMAGE_TYPE_2D, .format = fmt, .extent = {W, H, 1},
      .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED };
   VkMemoryRequirements mr;
   VkPhysicalDeviceMemoryProperties mp;
   VkDeviceMemory memory;
   CK(vkCreateImage(dev, &ici, NULL, &sample_image));
   vkGetImageMemoryRequirements(dev, sample_image, &mr);
   vkGetPhysicalDeviceMemoryProperties(pd, &mp);
   uint32_t t;
   for (t = 0; t < mp.memoryTypeCount; t++)
      if ((mr.memoryTypeBits & (1u << t)) && (mp.memoryTypes[t].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) break;
   if (t == mp.memoryTypeCount) FAIL("sample memory type");
   VkMemoryAllocateInfo mai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size, .memoryTypeIndex = t };
   CK(vkAllocateMemory(dev, &mai, NULL, &memory));
   CK(vkBindImageMemory(dev, sample_image, memory, 0));
   VkImageView view;
   VkImageViewCreateInfo vi = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = sample_image, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = fmt,
      .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
   CK(vkCreateImageView(dev, &vi, NULL, &view));
   VkAttachmentDescription att = { .format = fmt, .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
   VkAttachmentReference ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
   VkSubpassDescription sub = { .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = 1, .pColorAttachments = &ref };
   VkSubpassDependency dep = { .srcSubpass = VK_SUBPASS_EXTERNAL, .dstSubpass = 0,
      .srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT };
   VkRenderPassCreateInfo rpci = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = 1, .pAttachments = &att, .subpassCount = 1, .pSubpasses = &sub,
      .dependencyCount = 1, .pDependencies = &dep };
   CK(vkCreateRenderPass(dev, &rpci, NULL, &sample_pass));
   VkFramebufferCreateInfo fi = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass = sample_pass, .attachmentCount = 1, .pAttachments = &view, .width = W, .height = H, .layers = 1 };
   CK(vkCreateFramebuffer(dev, &fi, NULL, &sample_fb));
   VkSampler sampler;
   VkSamplerCreateInfo sci = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_NEAREST, .minFilter = VK_FILTER_NEAREST,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, .maxLod = 0 };
   CK(vkCreateSampler(dev, &sci, NULL, &sampler));
   VkDescriptorSetLayout dsl;
   VkDescriptorSetLayoutBinding binding = { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT };
   VkDescriptorSetLayoutCreateInfo dsci = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1, .pBindings = &binding };
   CK(vkCreateDescriptorSetLayout(dev, &dsci, NULL, &dsl));
   VkDescriptorPool pool;
   VkDescriptorPoolSize ps = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 };
   VkDescriptorPoolCreateInfo dpci = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &ps };
   CK(vkCreateDescriptorPool(dev, &dpci, NULL, &pool));
   VkDescriptorSetAllocateInfo dai = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = pool, .descriptorSetCount = 1, .pSetLayouts = &dsl };
   CK(vkAllocateDescriptorSets(dev, &dai, &sample_set));
   VkDescriptorImageInfo di = { sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
   VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = sample_set,
      .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &di };
   vkUpdateDescriptorSets(dev, 1, &write, 0, NULL);
   VkPipelineLayoutCreateInfo plci = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1, .pSetLayouts = &dsl };
   CK(vkCreatePipelineLayout(dev, &plci, NULL, &sample_layout));
   VkPipelineShaderStageCreateInfo stages[2] = {
      { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = sample_shader("sample-clear.vert.spv"), .pName = "main" },
      { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = sample_shader("sample-clear.frag.spv"), .pName = "main" } };
   VkPipelineVertexInputStateCreateInfo vertex = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
   VkPipelineInputAssemblyStateCreateInfo assembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
   VkViewport vp = { 0, 0, (float)W, (float)H, 0, 1 };
   VkRect2D rect = { {0, 0}, {W, H} };
   VkPipelineViewportStateCreateInfo viewport = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1, .pViewports = &vp, .scissorCount = 1, .pScissors = &rect };
   VkPipelineRasterizationStateCreateInfo raster = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE, .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, .lineWidth = 1 };
   VkPipelineMultisampleStateCreateInfo ms = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT };
   VkPipelineColorBlendAttachmentState blendatt = { .colorWriteMask = 15 };
   VkPipelineColorBlendStateCreateInfo blend = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = 1, .pAttachments = &blendatt };
   VkGraphicsPipelineCreateInfo gp = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = 2, .pStages = stages, .pVertexInputState = &vertex, .pInputAssemblyState = &assembly,
      .pViewportState = &viewport, .pRasterizationState = &raster, .pMultisampleState = &ms,
      .pColorBlendState = &blend, .layout = sample_layout, .renderPass = output_pass };
   CK(vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gp, NULL, &sample_pipeline));
}

static void sample_record(VkCommandBuffer cb, VkClearColorValue orange, int copy, VkBuffer buffer)
{
   VkClearValue cv = { .color = orange };
   VkRenderPassBeginInfo bi = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = sample_pass, .framebuffer = sample_fb, .renderArea = { {0, 0}, {W, H} },
      .clearValueCount = 1, .pClearValues = &cv };
   vkCmdBeginRenderPass(cb, &bi, VK_SUBPASS_CONTENTS_INLINE);
   vkCmdEndRenderPass(cb);
   VkImageMemoryBarrier b = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .dstAccessMask = copy ? VK_ACCESS_TRANSFER_READ_BIT : VK_ACCESS_SHADER_READ_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .newLayout = copy ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = sample_image, .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
   vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      copy ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &b);
   if (copy) {
      VkBufferImageCopy reg = { .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, .imageExtent = {W, H, 1} };
      vkCmdCopyImageToBuffer(cb, sample_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &reg);
      b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
      b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &b);
      VkBufferMemoryBarrier bb = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
         .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .buffer = buffer, .size = VK_WHOLE_SIZE };
      vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 1, &bb, 0, NULL);
   }
}
