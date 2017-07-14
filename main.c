#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

#include <graphene.h>

#include <glib.h>
#include <string.h>
#include <cairo.h>

#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define VULKAN_ERROR vulkan_error_quark ()
//shamelessly borrowed from cairo examples

static void
draw_clock (cairo_t *cr)
{
  time_t t;
  struct tm *tm;
  double seconds, minutes, hours;


  t = time(NULL);
  tm = localtime(&t);


  seconds = tm->tm_sec * M_PI / 30;
  minutes = tm->tm_min * M_PI / 30;
  hours = tm->tm_hour * M_PI / 6;

  cairo_save(cr);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba(cr, 1., 1., 1., 0.0);
  cairo_paint(cr);
  cairo_restore (cr);

  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_width(cr, 0.1);


  cairo_set_source_rgba(cr, .5, .5, .5, .8);
  cairo_translate(cr, 0.5, 0.5);
  cairo_arc(cr, 0, 0, 0.4, 0, M_PI * 2);
  cairo_stroke(cr);

  /* seconds */
  cairo_set_source_rgba(cr, 1, 1, 1, 0.6);
  cairo_arc(cr, sin(seconds) * 0.4, -cos(seconds) * 0.4,
            0.05, 0, M_PI * 2);
  cairo_fill(cr);

  /* minutes */
  cairo_set_source_rgba(cr, 0.2, 0.2, 1, 0.6);
  cairo_move_to(cr, 0, 0);
  cairo_line_to(cr, sin(minutes) * 0.4, -cos(minutes) * 0.4);
  cairo_stroke(cr);

  /* hours     */
  cairo_move_to(cr, 0, 0);
  cairo_line_to(cr, sin(hours) * 0.2, -cos(hours) * 0.2);
  cairo_stroke(cr);
}

GQuark
vulkan_error_quark (void)
{
  return g_quark_from_static_string ("vulkan-error-quark");
}

static GArray *
vkg_enum_instance_extension_properties(const char *layer_name, GError **error)
{
  uint32_t count = 0;
  VkResult res;
  GArray *props;

  res = vkEnumerateInstanceExtensionProperties(layer_name, &count, NULL);
  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "failed (I)");
      return NULL;
    }

  props = g_array_sized_new(FALSE, FALSE, sizeof(VkExtensionProperties), count);

  res = vkEnumerateInstanceExtensionProperties(layer_name,
					       &count,
					       (VkExtensionProperties *) props->data);
  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "failed (II)");
      g_array_free(props, TRUE);
      return NULL;
    }

  props->len = count;
  return props;
}

static GArray *
vkg_enum_instance_layer_props(GError **error)
{
  uint32_t count = 0;
  VkResult res;
  GArray *props;

  res = vkEnumerateInstanceLayerProperties(&count, NULL);

  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "failed (I)");
      return NULL;
    }

  props = g_array_sized_new(FALSE, FALSE, sizeof(VkLayerProperties), count);

  res = vkEnumerateInstanceLayerProperties(&count, (void *) props->data);

  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "failed (II)");
      g_array_free(props, TRUE);
      return NULL;
    }

  props->len = count;
  return props;
}

static gboolean
vkg_find_layer(GArray *props, const char *name, guint *index)
{

  for (guint i = 0; i < props->len; i++)
    {
      VkLayerProperties *lp = &g_array_index(props, VkLayerProperties, i);
      if (g_str_equal(name, lp->layerName))
	{
	  if (index)
	    *index = i;
	}
      return TRUE;
    }

  return FALSE;
}

static VkSemaphore
vkg_create_semaphore(VkDevice device,
		     VkSemaphoreCreateFlags flags,
		     GError **error)
{
  VkResult res;
  VkSemaphore sem;
  VkSemaphoreCreateInfo ci = {
    VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    NULL,
    flags
  };

  res = vkCreateSemaphore(device, &ci, NULL, &sem);

  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "failed");
      return VK_NULL_HANDLE;
    }

  return sem;
}


static GArray *
vkg_get_physical_device_surface_present_modes(VkPhysicalDevice device,
					      VkSurfaceKHR     surface,
					      GError         **error)
{
  VkResult res;
  uint32_t count = 0;
  GArray *modes;

  res = vkGetPhysicalDeviceSurfacePresentModesKHR(device,
						  surface,
						  &count,
						  NULL);
  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "failed (I)");
      return NULL;
    }

  modes = g_array_sized_new(FALSE, FALSE, sizeof(VkPresentModeKHR), count);

  res = vkGetPhysicalDeviceSurfacePresentModesKHR(device,
						  surface,
						  &count,
						  (void *) modes->data);

  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "failed (II)");
      g_array_free(modes, TRUE);
      return NULL;
    }

  modes->len = count;
  return modes;
}

static GArray *
vgk_get_physical_device_surface_formats(VkPhysicalDevice device,
					VkSurfaceKHR     surface,
					GError         **error)
{
  VkResult res;
  uint32_t count = 0;
  GArray *formats;

  res = vkGetPhysicalDeviceSurfaceFormatsKHR(device,
					     surface,
					     &count,
					     NULL);
  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "failed (I)");
      return NULL;
    }

  formats = g_array_sized_new(FALSE, FALSE, sizeof(VkSurfaceFormatKHR), count);

  res = vkGetPhysicalDeviceSurfaceFormatsKHR(device,
						  surface,
						  &count,
						  (void *) formats->data);

  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "failed (II)");
      g_array_free(formats, TRUE);
      return NULL;
    }

  formats->len = count;
  return formats;
}

static GArray *
vgk_get_swapchain_images(VkDevice device,
			 VkSwapchainKHR swapchain,
			 GError **error)
{
  VkResult res;
  uint32_t count = 0;
  GArray *images;

  res = vkGetSwapchainImagesKHR(device, swapchain, &count, NULL);
  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "failed (I)");
      return NULL;
    }

  images = g_array_sized_new(FALSE, FALSE, sizeof(VkImage), count);
  res = vkGetSwapchainImagesKHR(device,
				swapchain,
				&count,
				(void *) images->data);

  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "failed (I)");
      g_array_free(images, TRUE);
      return NULL;
    }

  images->len = count;
  return images;
}

static gboolean
vgk_auto_image_format(GArray *formats, //VkSurfaceFormatKHR
		      VkSwapchainCreateInfoKHR *info)
{
  const guint count = formats->len;
  const VkSurfaceFormatKHR *data = (VkSurfaceFormatKHR *) formats->data;

  if (count < 1)
    {
      return FALSE;
    }
  else if (count == 1 && data[0].format == VK_FORMAT_UNDEFINED)
    {
      info->imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
      info->imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
      return TRUE;
    }

  for (guint i = 0; i < count; i++)
    {
      if (data[i].format == VK_FORMAT_R8G8B8A8_UNORM)
	{
	  info->imageFormat = data[i].format;
	  info->imageColorSpace = data[i].colorSpace;
	  return TRUE;
	}
    }

   info->imageFormat = data[0].format;
   info->imageColorSpace = data[0].colorSpace;
   return TRUE;
}

static VkRenderPass
create_render_pass(VkDevice  device,
		   VkFormat  color_format,
		   VkFormat  depth_format,
		   GError  **error)
{
  VkResult res;
  VkRenderPass pass;

  VkAttachmentDescription attachments[2] = {
    { /* color */
      .format         = color_format,
      .samples        = VK_SAMPLE_COUNT_1_BIT,
      .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,      // what to do with image at the beginning of the pass
      .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,     // what to do with the image at the end of the pass
      .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // as above for stencil
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, // as above
      .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,        // layout at the start of the pass
      .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,  // transition to this layout at the end of the pass
    },
    { /* depth */
      .format         = depth_format,
      .samples        = VK_SAMPLE_COUNT_1_BIT,
      .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    }
  };

  VkAttachmentReference cref = {
    .attachment = 0,
    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  VkAttachmentReference dref = {
    .attachment = 1,
    .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };

  VkSubpassDescription subpass_desc = {
    .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS, // graphics or compute
    .colorAttachmentCount    = 1,
    .pColorAttachments       = &cref,
    .pDepthStencilAttachment = NULL, //TODO: &dref,
    .inputAttachmentCount    = 0,
    .pInputAttachments       = NULL,
    .preserveAttachmentCount = 0,
    .pPreserveAttachments    = NULL,
    .pResolveAttachments     = NULL,
  };

  VkSubpassDependency deps[2] = {
    {
      .srcSubpass      = VK_SUBPASS_EXTERNAL,
      .dstSubpass      = 0,
      .srcStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
      .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .srcAccessMask   = VK_ACCESS_MEMORY_READ_BIT,
      .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
    },
    {
      .dstSubpass      = VK_SUBPASS_EXTERNAL,
      .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .dstStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
      .srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT,
      .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
    }
  };

  VkRenderPassCreateInfo rp_info = {
    .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .attachmentCount = 1, //TODO:, depth 2,
    .pAttachments    = attachments,
    .subpassCount    = 1,
    .pSubpasses      = &subpass_desc,
    .dependencyCount = 2,
    .pDependencies   = deps,
  };


  res = vkCreateRenderPass(device, &rp_info, NULL, &pass);
  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "failed");
      return VK_NULL_HANDLE;
    }

  return pass;
}

static GArray *
vgk_swapchain_create_image_views(VkDevice  device,
				 GArray   *images,
				 VkFormat  format,
				 GError  **error)
{
  GArray   *views;
  VkResult  res;

  views = g_array_sized_new(FALSE, FALSE, sizeof(VkImageView), images->len);
  //TODO: free func?

  for (guint i = 0; i < images->len; i++)
    {
      VkImage img = g_array_index(images, VkImage, i);
      VkImageView *view = &g_array_index(views, VkImageView, i);

      VkImageViewCreateInfo ci = {
	.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
	.pNext                           = NULL,
	.format                          = format,
	.components = {
	  VK_COMPONENT_SWIZZLE_R,
	  VK_COMPONENT_SWIZZLE_G,
	  VK_COMPONENT_SWIZZLE_B,
	  VK_COMPONENT_SWIZZLE_A
	},
	.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
	.subresourceRange.baseMipLevel   = 0,
	.subresourceRange.levelCount     = 1,
	.subresourceRange.baseArrayLayer = 0,
	.subresourceRange.layerCount     = 1,
	.viewType                        = VK_IMAGE_VIEW_TYPE_2D,
	.flags                           = 0,
	.image                           = img,
      };

      res = vkCreateImageView(device, &ci, NULL, view);

      if (res != VK_SUCCESS)
	{
	  g_array_free(views, TRUE);
	  g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			      "failed");
	  return NULL;
	}

      views->len = i + 1;
    }

  return views;
}


static VkShaderModule
load_shader(VkDevice     device,
	    const char  *path,
	    GError     **error)
{
  VkResult res;
  char *data;
  gsize len;
  gboolean ok;

  ok = g_file_get_contents(path, &data, &len, error);
  if (!ok)
    {
      return VK_NULL_HANDLE;
    }

  VkShaderModuleCreateInfo ci = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .pNext    = NULL,
    .flags    = 0,
    .codeSize = len,
    .pCode    = (uint32_t *) data,
  };

  VkShaderModule module;
  res = vkCreateShaderModule(device, &ci, NULL, &module);

  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res, "failed");
      return VK_NULL_HANDLE;
    }

  return module;
}

gboolean
vkg_auto_mem_type_index(VkMemoryRequirements *mreq,
			VkPhysicalDeviceMemoryProperties dp,
			VkMemoryPropertyFlags props,
			VkMemoryAllocateInfo *ai)
{
  uint32_t bits = mreq->memoryTypeBits;

  for (uint32_t i = 0; i < dp.memoryTypeCount; i++)
    {
      if ((bits & 1) == 1)
	{
	  if ((dp.memoryTypes[i].propertyFlags & props) == props)
	    {
	      ai->memoryTypeIndex = i;
	      return TRUE;
	    }
	}
      bits >>= 1;
    }

  return FALSE;
}

typedef struct VkGStagingArea_ {
  VkDeviceMemory staging_memory;
  VkBuffer       staging_buffer;

  VkDeviceMemory device_memory;
  VkBuffer       device_buffer;
} VkGStagingArea;

gboolean
vkg_memcpy_stage(VkDevice dev,
		 VkPhysicalDevice phy,
		 void *data, size_t size,
		 VkBufferUsageFlags usage,
		 VkGStagingArea *area,
		 GError **error)
{
  VkResult res;
  VkMemoryRequirements mreq = { };

  VkPhysicalDeviceMemoryProperties dev_mem_props;
  vkGetPhysicalDeviceMemoryProperties(phy, &dev_mem_props);

  VkBufferCreateInfo bci = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size  = size,
    .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
  };

  VkDeviceMemory staging_memory;
  VkBuffer       staging_buffer;

  res = vkCreateBuffer(dev, &bci, NULL, &staging_buffer);

  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "staging buffer creation failed");
      return FALSE;
    }

  vkGetBufferMemoryRequirements(dev, staging_buffer, &mreq);

  VkMemoryAllocateInfo ai = {
    .sType          = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = mreq.size,
  };

  gboolean ok = vkg_auto_mem_type_index(&mreq,
					dev_mem_props,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
					VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					&ai);

  if (!ok)
    {
      return FALSE;
    }

  res = vkAllocateMemory(dev, &ai, NULL, &staging_memory);

  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "staging buffer memory allocation failed");
      return 0;
    }

  void *mapped;
  res = vkMapMemory(dev, staging_memory, 0, ai.allocationSize, 0, &mapped);
  memcpy(mapped, data, size);
  vkUnmapMemory(dev, staging_memory);

  res = vkBindBufferMemory(dev, staging_buffer, staging_memory, 0);

  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "staging buffer binding failed");
      return 0;
    }

  bci.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

  VkDeviceMemory device_memory;
  VkBuffer       device_buffer;

  res = vkCreateBuffer(dev, &bci, NULL, &device_buffer);

  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "device buffer creation failed");
      return FALSE;
    }

  vkGetBufferMemoryRequirements(dev, device_buffer, &mreq);

  ok = vkg_auto_mem_type_index(&mreq,
			       dev_mem_props,
			       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			       &ai);

  if (!ok)
    {
      return FALSE;
    }

  res = vkAllocateMemory(dev, &ai, NULL, &device_memory);

  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "allocating device memory failed");
      return FALSE;
    }

  res = vkBindBufferMemory(dev, device_buffer, device_memory, 0);

  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "binding device buffer failed");
      return FALSE;
    }

  area->staging_memory = staging_memory;
  area->staging_buffer = staging_buffer;

  area->device_memory = device_memory;
  area->device_buffer = device_buffer;

  return TRUE;
}

VkCommandBuffer
vkg_command_buffer_get(VkDevice dev,
		       VkCommandPool cp,
		       gboolean begin,
		       GError **error)
{
  VkResult res;
  VkCommandBuffer buffer[1];

  VkCommandBufferAllocateInfo info = {
    .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool        = cp,
    .commandBufferCount = 1,
    .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
  };

  res = vkAllocateCommandBuffers(dev, &info, buffer);

  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "allocation failed");
      return VK_NULL_HANDLE;
    }

  if (begin)
    {
      VkCommandBufferBeginInfo cb_begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
      };

      res = vkBeginCommandBuffer(buffer[0], &cb_begin);
      if (res != VK_SUCCESS)
	{
	  g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			      "begin buffer failed");
	  return VK_NULL_HANDLE;
	}
    }

  return buffer[0];
}

gboolean
vkg_command_buffer_flush(VkDevice dev,
			 VkQueue queue,
			 VkCommandPool cp,
			 VkCommandBuffer buffer,
			 GError **error)
{
  VkResult res;

  res = vkEndCommandBuffer(buffer);
  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "end command buffer faild\n");
      return FALSE;
    }

  VkFenceCreateInfo fi = {
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    .flags = 0,
  };

  VkFence fence;
  res = vkCreateFence(dev, &fi, NULL, &fence);

  VkSubmitInfo si = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = 1,
    .pCommandBuffers = &buffer,
  };

  res = vkQueueSubmit(queue, 1, &si, fence);
  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "queue submission failed\n");
      return FALSE;
    }

  res = vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX);
  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "waiting for fence\n");
      return FALSE;
    }

  vkDestroyFence(dev, fence, NULL);
  vkFreeCommandBuffers(dev, cp, 1, &buffer);

  return TRUE;
}

typedef struct UniData_ {
  graphene_matrix_t projection;
  graphene_matrix_t model;
  graphene_matrix_t view;
} UniData;

typedef struct Uni_ {
  VkDeviceMemory         memory;
  VkBuffer               buffer;
  VkDescriptorBufferInfo descriptor;

  struct _ {
    graphene_matrix_t projection;
    graphene_matrix_t model;
    graphene_matrix_t view;
  } data;

  size_t size;
} Uni;

static gboolean
update_uni_data(VkDevice             dev,
		Uni                 *uni,
		VkExtent2D           ext,
		float                zoom,
		graphene_point3d_t   rot,
		GError             **error)
{
  VkResult res;

  graphene_matrix_t *projection = &uni->data.projection;
  graphene_matrix_t *model = &uni->data.model;
  graphene_matrix_t *view = &uni->data.view;
  graphene_point3d_t pos = GRAPHENE_POINT3D_INIT(0.f, 0.f, zoom);

  float aspect = (float) ext.width / (float) ext.height;
  graphene_matrix_init_perspective(projection,
				   60.f,
				   aspect,
				   .1f,
				   256.f);

  graphene_matrix_init_identity(view);
  graphene_matrix_translate(view, &pos);

  graphene_matrix_init_identity(model);
  graphene_matrix_rotate_x(model, rot.x);
  graphene_matrix_rotate_y(model, rot.y);
  graphene_matrix_rotate_z(model, rot.z);

  void *data;
  res = vkMapMemory(dev, uni->memory, 0, uni->size, 0, &data);

  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "mapping failed");
      return FALSE;
    }

#if 0
  g_print("\n");
  graphene_matrix_print(projection);
  g_print("\n");
  graphene_matrix_print(view);
  g_print("\n");
  graphene_matrix_print(model);
#endif

  size_t es = 16 * sizeof(float);
  graphene_matrix_to_float(projection, data + 0 * es);
  graphene_matrix_to_float(model,      data + 1 * es);
  graphene_matrix_to_float(view,       data + 2 * es);

  vkUnmapMemory(dev, uni->memory);

  return TRUE;
}

static void
vkg_transition_layout(VkCommandBuffer cmd_buf,
		      VkImage         image,
		      VkFormat        format,
		      VkImageLayout   from,
		      VkImageLayout   to)
{
  VkImageMemoryBarrier barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .oldLayout = from,
    .newLayout = to,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = image,
    .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .subresourceRange.baseMipLevel = 0,
    .subresourceRange.levelCount = 1,
    .subresourceRange.baseArrayLayer = 0,
    .subresourceRange.layerCount = 1,
  };

  if (from == VK_IMAGE_LAYOUT_PREINITIALIZED &&
      to   == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {

    barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

  } else if (from == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
	     to   == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  } else if (from == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
	     to   == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {

    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

  } else {
    g_error("Invalid transition of image layouts");
  }

  vkCmdPipelineBarrier(cmd_buf,
		       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		       0,
		       0, NULL,
		       0, NULL,
		       1, &barrier);
}

static gboolean
update_texture_with_clock(VkDevice dev,
			  VkDeviceMemory  mem,
			  VkDeviceSize    mem_size,
			  int width,
			  int height,
			  int stride)
{
  void *data;
  VkResult res = vkMapMemory(dev, mem, 0, mem_size, 0, &data);

  if (res != VK_SUCCESS)
    {
      g_print("[E] could not map memory");
      return FALSE;
    }

  cairo_surface_t *tex_surface =
    cairo_image_surface_create_for_data(data,
                                        CAIRO_FORMAT_ARGB32,
                                        width,
                                        height,
                                        stride);

  cairo_t *cr = cairo_create(tex_surface);

  cairo_scale(cr, width, height);
  draw_clock(cr);
  cairo_destroy(cr);
  cairo_surface_flush(tex_surface);
  vkUnmapMemory(dev, mem);

  cairo_surface_destroy(tex_surface);

  return TRUE;
}
/* ************************************************************************ */
int main(int argc, char **argv)
{
  gboolean enable_validation = TRUE;
  VkResult res;
  gboolean ok;
  g_autoptr(GError) err = NULL;

  res = (VkResult) glfwInit();

  if (!res) {
    g_print("[E] glfw could not be initialized!");
    return -1;
  }

  if (! glfwVulkanSupported()) {
    g_print("[E] glfw reports no vulkan support.  (╯°□°）╯︵ ┻━┻ ");
    return -1;
  }

  GArray *extensions = vkg_enum_instance_extension_properties(NULL, &err);
  if (!extensions)
    {
      g_print("could not enumerate extensions: %s", err->message);
      return -1;
    }

  g_print(" o-Extensions: [%u]\n", extensions->len);
  for (guint i = 0; i < extensions->len; i++)
    {
      VkExtensionProperties *prop = &g_array_index(extensions, VkExtensionProperties, i);
      g_print("   |- %s\n", prop->extensionName);
    }

  GArray *layers = vkg_enum_instance_layer_props(&err);
  if (!layers)
    {
      g_print("could not enumerate layers: %s", err->message);
      return -1;
    }

  g_print(" o-layers: [%u]\n", layers->len);
  for (guint i = 0; i < layers->len; i++)
    {
      VkLayerProperties *prop = &g_array_index(layers, VkLayerProperties, i);
      g_print("   |- %s\n", prop->layerName);
    }

  uint32_t ext_count;
  const char **ext_names = glfwGetRequiredInstanceExtensions(&ext_count);

  g_print(" o-Required extensions by gflw:\n");
  for (uint32_t i = 0; i < ext_count; i++)
    {
      g_print(" - %s\n", ext_names[i]);
    }

  g_autoptr(GPtrArray) enabled_layers = g_ptr_array_new_with_free_func(g_free);

  if (enable_validation)
    {
      const char *name = "VK_LAYER_LUNARG_standard_validation";
      if (!vkg_find_layer(layers, name, NULL))
	{
	  g_print("[E] Could not enable validation!\n");
	  return -1;
	}

      g_ptr_array_add(enabled_layers, g_strdup(name));
    }

  VkInstanceCreateInfo ic_info = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .pApplicationInfo = NULL,
    .enabledLayerCount = enabled_layers->len,
    .ppEnabledLayerNames = (const char *const *) enabled_layers->pdata,
    .enabledExtensionCount = ext_count,
    .ppEnabledExtensionNames = ext_names,
  };

  VkInstance inst;
  res = vkCreateInstance(&ic_info, NULL, &inst);

  if (res != VK_SUCCESS)
    {
      return -1;
    }

  uint32_t dev_count;
  res = vkEnumeratePhysicalDevices(inst, &dev_count, NULL);

  if (res != VK_SUCCESS)
    {
      return -1;
    }

  VkPhysicalDevice *devices = g_new(VkPhysicalDevice, dev_count);
  res = vkEnumeratePhysicalDevices(inst, &dev_count, devices);

  if (res != VK_SUCCESS)
    {
      return -1;
    }

  uint32_t i, k;
  VkDevice dev;
  uint32_t dev_idx, qf_idx;
  gboolean have_device = FALSE;
  dev_idx = qf_idx = 0;

  g_print("Devices:\n");
  for (i = 0; i < dev_count; i++)
    {
       VkPhysicalDeviceProperties props;
       vkGetPhysicalDeviceProperties(devices[i], &props);

       g_print(" o-%s\n", props.deviceName);
       g_print("   |-API: %d.%d.%d\n",
               VK_VERSION_MAJOR(props.apiVersion),
               VK_VERSION_MINOR(props.apiVersion),
               VK_VERSION_PATCH(props.apiVersion));

       uint32_t qf_count;
       vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &qf_count, NULL);
       VkQueueFamilyProperties *qf_props = g_newa(VkQueueFamilyProperties, qf_count);
       vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &qf_count, qf_props);

       for (k = 0; k < qf_count; k++)
         {
           gboolean have_gfx = qf_props[k].queueFlags & VK_QUEUE_GRAPHICS_BIT;
           gboolean have_compute = qf_props[k].queueFlags & VK_QUEUE_COMPUTE_BIT;
           gboolean have_transfer = qf_props[k].queueFlags & VK_QUEUE_TRANSFER_BIT;

           g_print("   | Queue [%u/%u] \n", k, qf_count);
           g_print("   | |- graphics: %s\n", have_gfx ? "yes" : "no");
           g_print("   | |- compute: %s\n", have_compute ? "yes" : "no");
           g_print("   | |- transfer: %s\n", have_transfer ? "yes" : "no");

           if (!have_device && have_gfx)
             {
               VkDeviceQueueCreateInfo qc_info = {
                 .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                 .queueFamilyIndex = k,
                 .queueCount = 1,
                 .pQueuePriorities = (float []) { 1.0f },
                 .pNext = NULL,
                 .flags = 0,
               };

               VkDeviceCreateInfo dc_info = {
                 .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                 .queueCreateInfoCount = 1,
                 .pQueueCreateInfos = &qc_info,
                 .enabledExtensionCount = 1,
                 .ppEnabledExtensionNames = (const char * const []) {
                   VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                 },
                 .pNext = NULL,
                 .flags = 0,
                 .enabledLayerCount = 0,
                 .ppEnabledLayerNames = NULL,
                 .pEnabledFeatures = NULL,
               };

               res = vkCreateDevice(devices[i], &dc_info, NULL, &dev);
               have_device = res == VK_SUCCESS;

               g_print("   |  `- device: %s [%p]\n",
                       have_device ? "OK" : ":(",
                       have_device ? dev : 0x0);

               if (have_device)
                 {
                   dev_idx = i;
                   qf_idx = k;
                   break;
                 }
             }
         }
    }

  if (!have_device)
    {
      g_print("[E] no suitable gpu found.");
      return -1;
    }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow *win = glfwCreateWindow(640, 480, "vkpg", NULL, NULL);

  VkSurfaceKHR surface;
  res = glfwCreateWindowSurface(inst, win, NULL, &surface);

  g_print("   o-surface: ");
  if (res != VK_SUCCESS)
    {
      g_print("[E] could not get a surface .. \n");
      return -1;
    }
  // ok, we got ourselves a vulkan surface
    g_print(" ok \n");

  VkQueue queue;
  vkGetDeviceQueue(dev, qf_idx, 0, &queue);

  VkBool32 dss;
  res = vkGetPhysicalDeviceSurfaceSupportKHR(devices[dev_idx],
					     qf_idx,
					     surface,
					     &dss);

  if (res != VK_SUCCESS)
    {
      g_print(" [E] vkGetPhysicalDeviceSurfaceSupportKHR failed!\n");
      return -1;
    }

  g_print("     |- physical device surface support: %s\n",
	  dss ? "yes" : "no");

  VkSurfaceCapabilitiesKHR sc;
  res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(devices[dev_idx],
                                                  surface,
                                                  &sc);
  if (res != VK_SUCCESS)
    {
      return -1;
    }

  g_print("     |- size: %u x %u\n",
          sc.currentExtent.width, sc.currentExtent.height);

  g_print("     |- images: %u <> %u\n",
          sc.minImageCount, sc.maxImageCount);

  g_print("     o- usage:\n");
  g_print("        |- %s: %s \n",
	  "color attachment",
	  sc.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT ? "yes" : "no");
  g_print("        |- %s: %s \n",
	  "transfer dst",
	  sc.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT ? "yes" : "no");
  g_print("        |- %s: %s \n",
	  "storage",
	  sc.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT ? "yes" : "no");
  g_print("        |- %s: %s \n",
	  "sampled",
	  sc.supportedUsageFlags & VK_IMAGE_USAGE_SAMPLED_BIT ? "yes" : "no");

  g_print("     o- formats:\n");
  g_autoptr(GArray) formats = vgk_get_physical_device_surface_formats(devices[dev_idx],
								      surface,
								      &err);
  if (formats == NULL)
    {
      g_print("     [E] Could not get surface formats: %s", err->message);
      return -1;
    }

  for (guint i = 0; i < formats->len; i++)
    {
      const VkSurfaceFormatKHR *sf = &g_array_index(formats, VkSurfaceFormatKHR, i);
      g_print("        |- %d, %d \n", sf->format, sf->colorSpace);
    }


  g_autoptr(GArray) pmodes = vkg_get_physical_device_surface_present_modes(devices[dev_idx], surface, NULL);
  g_print("     o-presentation modes:\n");
  for (guint i = 0; i < pmodes->len; i++)
    {
      const VkPresentModeKHR *mode = &g_array_index(pmodes, VkPresentModeKHR, i);
      g_print("        |- %d \n", *mode);
    }

  uint32_t img_count = MAX(sc.minImageCount, 3);
  if (sc.maxImageCount > 0)
    {
      img_count = MIN(img_count, sc.maxImageCount);
    }

  VkSwapchainCreateInfoKHR scc_info = {
    .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .surface          = surface,
    .minImageCount    = img_count,
    .imageExtent      = sc.currentExtent,
    .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, //FIXME test for the support first
    .preTransform     = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
    .imageArrayLayers = 1,
    .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .presentMode      = VK_PRESENT_MODE_MAILBOX_KHR,
    .clipped          = TRUE,
    .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
  };

  vgk_auto_image_format(formats, &scc_info);
  VkFormat format = scc_info.imageFormat;

  g_print("   o-swapchain: ");
  VkSwapchainKHR swapchain;
  res = vkCreateSwapchainKHR(dev, &scc_info, NULL, &swapchain);
  if (res != VK_SUCCESS)
    {
      g_print("[E] could not create swap chain .. \n");
      return -1;
    }
  g_print(" ok \n");

  uint32_t sci_count;
  res = vkGetSwapchainImagesKHR(dev, swapchain, &sci_count, NULL);
  if (res != VK_SUCCESS)
    {
      g_print("[E] could not get swapchain image count .. \n");
      return -1;
    }
  g_print("     |- images: %u [%u]\n", sci_count, img_count);

  g_autoptr(GArray) imgs = vgk_get_swapchain_images(dev, swapchain, &err);
  if (imgs == NULL)
    {
      g_print("[E] could not get swapchain images .. \n");
      return -1;
    }

  /* ************************************************************************ */

  VkCommandPoolCreateInfo cpc_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .queueFamilyIndex = qf_idx,
  };

  g_print("   o-command pool: ");

  VkCommandPool cp;
  res = vkCreateCommandPool(dev, &cpc_info, NULL, &cp);
  if (res != VK_SUCCESS)
    {
      g_print("[E] could not create command pool .. \n");
      return -1;
    }
  g_print("ok \n");

  VkCommandBufferAllocateInfo cba_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = cp,
    .commandBufferCount = sci_count,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
  };

  g_print("   o-command buffers: ");

  VkCommandBuffer *cmd_buf = g_new0(VkCommandBuffer, sci_count);
  res = vkAllocateCommandBuffers(dev, &cba_info, cmd_buf);
  if (res != VK_SUCCESS)
    {
      g_print("[E] could not create command buffers .. \n");
      return -1;
    }

  g_print("ok \n");
  /* ************************************************************************ */
  g_print("   o-render pass: ");
  VkRenderPass pass = create_render_pass(dev, format, format, &err);
  if (pass == VK_NULL_HANDLE)
    {
      g_print("[E] render pass creation: %s", err->message);
      return -1;
    }
  g_print("ok \n");
  /* ************************************************************************ */
  g_print("   o-pipeline cache: ");
  VkPipelineCache pipeline_cache;

  VkPipelineCacheCreateInfo pc_ci = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
  };

  res = vkCreatePipelineCache(dev, &pc_ci, NULL, &pipeline_cache);
  if (res != VK_SUCCESS)
    {
      g_print("[E] pipeline cache creation failed");
      return -1;
    }


  g_print("ok \n");
  /* ************************************************************************ */
  g_print("   o-image views: ");
  GArray *views = vgk_swapchain_create_image_views(dev, imgs, format, &err);

    if (views == NULL)
    {
      g_print("[E] image views creation: %s", err->message);
      return -1;
    }

  g_print("ok \n");
  /* ************************************************************************ */
  g_print("   o-framebuffer: ");

  GArray *framebuffers = g_array_sized_new(FALSE,
					   FALSE,
					   sizeof(VkFramebuffer),
					   imgs->len);

  for (guint i = 0; i < imgs->len; i++)
    {
      g_print("[%d] ", i);

      VkFramebuffer *fb = &g_array_index(framebuffers, VkFramebuffer, i);

      VkFramebufferCreateInfo fb_ci = {
	.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
	.pNext           = NULL,
	.renderPass      = pass,
	.attachmentCount = 1,
	.pAttachments    = &g_array_index(views, VkImageView, i),
	.width           = sc.currentExtent.width,
	.height          = sc.currentExtent.height,
	.layers          = 1,
      };

      res = vkCreateFramebuffer(dev, &fb_ci, NULL, fb);
      if (res != VK_SUCCESS)
	{
	  g_array_free(framebuffers, TRUE);
	  framebuffers = NULL;
	  g_set_error_literal(&err, VULKAN_ERROR, (gint) res,
			      "failed");
	  break;
	}

      framebuffers->len = i + 1;
    }
  if (framebuffers == NULL)
    {
      g_print("[E] frame buffer creation: %s", err->message);
      return -1;
    }
  g_print("ok \n");
  /* ************************************************************************ */
  g_print("   o-vertex data: ");
  VkPhysicalDeviceMemoryProperties dev_mem_props;
  vkGetPhysicalDeviceMemoryProperties(devices[dev_idx], &dev_mem_props);

  float box[] = { -0.5f,  0.5f,
		  -0.5f, -0.5f,
		   0.5f, -0.5f,

		   0.5f,  0.5f,
		  -0.5f,  0.5f,
		   0.5f, -0.5f};

  uint32_t box_idx[] = {0, 1, 2, 3, 4, 5};

  VkGStagingArea vertex_area;

  ok = vkg_memcpy_stage(dev,
			devices[dev_idx],
			box,
			sizeof(box),
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			&vertex_area, &err);

  if (!ok)
    {
      g_print("[E] copying vertex data: %s", err->message);
      return -1;
    }

  VkGStagingArea index_area;
  ok = vkg_memcpy_stage(dev,
			devices[dev_idx],
			box_idx,
			sizeof(box_idx),
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			&index_area, &err);

  if (!ok)
    {
      g_print("[E] copying vertex data: %s", err->message);
      return -1;
    }

  VkCommandBuffer copy_cmd = vkg_command_buffer_get(dev, cp, TRUE, &err);
  if (copy_cmd == VK_NULL_HANDLE)
    {
      g_print("[E] getting buffer: %s", err->message);
      return -1;
    }

  VkBufferCopy copy_region = {
    .size = sizeof(box),
  };

  vkCmdCopyBuffer(copy_cmd,
		  vertex_area.staging_buffer,
		  vertex_area.device_buffer,
		  1,
		  &copy_region);

  copy_region.size = sizeof(box_idx);

  vkCmdCopyBuffer(copy_cmd,
		  index_area.staging_buffer,
		  index_area.device_buffer,
		  1,
		  &copy_region);

  ok = vkg_command_buffer_flush(dev, queue, cp, copy_cmd, &err);

  if (!ok)
    {
      g_print("[E] flushing the buffer: %s", err->message);
      return -1;
    }

  vkDestroyBuffer(dev, vertex_area.staging_buffer, NULL);
  vkFreeMemory(dev, vertex_area.staging_memory, NULL);
  vkDestroyBuffer(dev, index_area.staging_buffer, NULL);
  vkFreeMemory(dev, index_area.staging_memory, NULL);

  g_print("ok \n");
  /* ************************************************************************ */
  g_print("   o-texture \n");

  size_t tex_size = 1024;
  int tex_height = tex_size, tex_width = tex_size;
  int stride = -1;
  VkDeviceSize tex_mem_size;

  g_print("     o-size: %lu \n", tex_size);
  stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, tex_width);
  tex_mem_size = tex_height * stride;

  g_print("     o-stride: %i \n", stride);
  g_print("     o-mem size: %lu \n", tex_mem_size);

  VkMemoryRequirements tex_mreq = { };

  VkBufferCreateInfo tex_bci = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size  = tex_mem_size,
    .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };

  VkDeviceMemory tex_staging_memory;
  VkBuffer       tex_staging_buffer;

  res = vkCreateBuffer(dev, &tex_bci, NULL, &tex_staging_buffer);

  if (res != VK_SUCCESS)
    {
      g_print("staging buffer creation failed");
      return -1;
    }

  vkGetBufferMemoryRequirements(dev, tex_staging_buffer, &tex_mreq);

  VkMemoryAllocateInfo tex_ai = {
    .sType          = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = tex_mreq.size,
  };

  ok = vkg_auto_mem_type_index(&tex_mreq,
			       dev_mem_props,
			       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			       &tex_ai);

  res = vkAllocateMemory(dev, &tex_ai, NULL, &tex_staging_memory);

  if (res != VK_SUCCESS)
    {
      g_print("staging buffer memory allocation failed");
      return -1;
    }

  res = vkBindBufferMemory(dev, tex_staging_buffer, tex_staging_memory, 0);

  if (res != VK_SUCCESS)
    {
      g_print("staging buffer binding failed");
      return -1;
    }


  /* initial texture transfer */
  ok = update_texture_with_clock(dev,
				 tex_staging_memory,
				 tex_mem_size,
				 tex_width, tex_height,
				 stride);

  if (!ok)
    {
      g_print("could not draw to the clock");
      return -1;
    }
  VkImage tex_image;
  VkDeviceMemory tex_image_memory;

  VkImageCreateInfo tex_ici = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .extent.width = tex_width,
    .extent.height = tex_height,
    .extent.depth = 1,
    .mipLevels = 1,
    .arrayLayers = 1,

    .format = VK_FORMAT_B8G8R8A8_UNORM,
    .tiling = VK_IMAGE_TILING_OPTIMAL,

    .initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED,
    .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
             VK_IMAGE_USAGE_SAMPLED_BIT,

    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .flags = 0,
  };

  res = vkCreateImage(dev, &tex_ici, NULL, &tex_image);

  if (res != VK_SUCCESS)
    {
      g_print("tex image creation failed\n");
      return -1;
    }

  vkGetImageMemoryRequirements(dev, tex_image, &tex_mreq);

  tex_ai.allocationSize = tex_mreq.size;
  ok = vkg_auto_mem_type_index(&tex_mreq,
			       dev_mem_props,
			       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			       &tex_ai);

  if (!ok)
    {
      g_print("Could not find suitable memory\n");
      return -1;
    }

  res = vkAllocateMemory(dev, &tex_ai, NULL, &tex_image_memory);
  if (res != VK_SUCCESS)
    {
      g_print("could not allocation memory for texture\n");
      return -1;
    }

  vkBindImageMemory(dev, tex_image, tex_image_memory, 0);

  copy_cmd = vkg_command_buffer_get(dev, cp, TRUE, &err);
  if (copy_cmd == VK_NULL_HANDLE)
    {
      g_print("[E] getting buffer: %s", err->message);
      return -1;
    }

  VkBufferImageCopy tex_region = {
    .bufferOffset = 0,
    /* assume we are tightly packed */
    .bufferRowLength = 0,
    .bufferImageHeight = 0,

    .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .imageSubresource.mipLevel = 0,
    .imageSubresource.baseArrayLayer = 0,
    .imageSubresource.layerCount = 1,

    .imageOffset = {0, 0, 0},
    .imageExtent = {tex_width, tex_height, 1},
  };

  vkg_transition_layout(copy_cmd,
			tex_image,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_LAYOUT_PREINITIALIZED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  vkCmdCopyBufferToImage(copy_cmd,
			 tex_staging_buffer,
			 tex_image,
			 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			 1,
			 &tex_region);

  vkg_transition_layout(copy_cmd,
			tex_image,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  ok = vkg_command_buffer_flush(dev, queue, cp, copy_cmd, &err);

  if (!ok)
    {
      g_print("[E] flushing the buffer: %s", err->message);
      return -1;
    }

  VkImageViewCreateInfo tex_ci = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .pNext                           = NULL,
    .format                          = VK_FORMAT_B8G8R8A8_UNORM,
    .components = {
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY
    },
    .subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
    .subresourceRange.baseMipLevel   = 0,
    .subresourceRange.levelCount     = 1,
    .subresourceRange.baseArrayLayer = 0,
    .subresourceRange.layerCount     = 1,
    .viewType                        = VK_IMAGE_VIEW_TYPE_2D,
    .flags                           = 0,
    .image                           = tex_image,
  };

  VkImageView tex_image_view;
  res = vkCreateImageView(dev, &tex_ci, NULL, &tex_image_view);

  if (res != VK_SUCCESS)
    {
      return -1;
    }

  VkSamplerCreateInfo tex_sci = {
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .magFilter               = VK_FILTER_LINEAR,
    .minFilter               = VK_FILTER_LINEAR,
    .addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .anisotropyEnable        = VK_FALSE,
    .maxAnisotropy           = 1,
    .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
    .unnormalizedCoordinates = VK_FALSE,
    .compareEnable           = VK_FALSE,
    .compareOp               = VK_COMPARE_OP_ALWAYS,
    .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
  };

  VkSampler tex_sampler;
  res = vkCreateSampler(dev, &tex_sci, NULL, &tex_sampler);

  if (res != VK_SUCCESS)
    {
      g_print("[E] could not create sampler\n");
      return -1;
    }

  g_print("     ok \n");
  /* ************************************************************************ */
  g_print("   o-uniform: ");
  Uni uni;
  uni.size = sizeof(float) * 16 * 3;

  VkBufferCreateInfo uni_bci = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = uni.size, // 4x4 matrix, 3 times
    .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
  };

  res = vkCreateBuffer(dev, &uni_bci, NULL, &uni.buffer);

  VkMemoryRequirements uni_mreq;
  vkGetBufferMemoryRequirements(dev, uni.buffer, &uni_mreq);

  VkMemoryAllocateInfo uni_ai = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .pNext           = NULL,
    .allocationSize  = uni_mreq.size,
    .memoryTypeIndex = 0,
  };

  ok = vkg_auto_mem_type_index(&uni_mreq,
			       dev_mem_props,
			       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			       &uni_ai);

  res = vkAllocateMemory(dev, &uni_ai, NULL, &uni.memory);
  if (res != VK_SUCCESS)
    {
      g_print("[E] could not allocate memory\n");
      return -1;
    }

  res = vkBindBufferMemory(dev, uni.buffer, uni.memory, 0);
  if (res != VK_SUCCESS)
    {
      g_print("[E] could not bind memory\n");
      return -1;
    }

  uni.descriptor.buffer = uni.buffer;
  uni.descriptor.offset = 0;
  uni.descriptor.range = uni.size;

  float               zoom = -2.5f;
  graphene_point3d_t  rotation = GRAPHENE_POINT3D_INIT(0.f, 0.f, 0.f);

  ok = update_uni_data(dev, &uni, sc.currentExtent, zoom, rotation, &err);

  if (!ok)
    {
      g_print("[E] updating uniform: %s\n", err->message);
    }

  g_print("ok \n");
  /* ************************************************************************ */
  g_print("   o-descriptor layout: ");

  VkDescriptorSetLayoutBinding layout_binding[] = {
    {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
      .pImmutableSamplers = NULL,
    },{
      .binding = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .pImmutableSamplers = NULL,
    }
  };

  VkDescriptorSetLayoutCreateInfo layout_desc = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pNext = NULL,
    .bindingCount = 2,
    .pBindings = layout_binding,
  };

  VkDescriptorSetLayout ds_layout;
  res = vkCreateDescriptorSetLayout(dev, &layout_desc, NULL, &ds_layout);
  if (res != VK_SUCCESS)
    {
      g_print("[E] could not create descriptor set layout\n");
      return -1;
    }

  VkPipelineLayoutCreateInfo pl_ci = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .pNext = NULL,
    .setLayoutCount = 1,
    .pSetLayouts = &ds_layout,
  };

  VkPipelineLayout pipeline_layout;
  res = vkCreatePipelineLayout(dev, &pl_ci, NULL, &pipeline_layout);
  if (res != VK_SUCCESS)
    {
      g_print("[E] could not create pipeline layout\n");
      return -1;
    }

  g_print("ok \n");
  /* ************************************************************************ */
  g_print("   o-descriptor pool: ");

  VkDescriptorPoolSize dps[] = {
    {
      .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
    },
    {
      .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
    },
  };


  VkDescriptorPoolCreateInfo dsp_ci = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .pNext = NULL,
    .poolSizeCount = 2,
    .pPoolSizes = dps,
    .maxSets = 1,
  };

  VkDescriptorPool desc_pool;
  res = vkCreateDescriptorPool(dev, &dsp_ci, NULL, &desc_pool);
  if (res != VK_SUCCESS)
    {
      g_print("[E] could not create pipeline layout\n");
      return -1;
    }

  g_print("ok \n");
  /* ************************************************************************ */
  g_print("   o-descriptor set: ");

  VkDescriptorSetAllocateInfo ds_ai = {
    .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool     = desc_pool,
    .descriptorSetCount = 1,
    .pSetLayouts        = &ds_layout,
  };

  VkDescriptorSet desc_set;
  res = vkAllocateDescriptorSets(dev, &ds_ai, &desc_set);
  if (res != VK_SUCCESS)
    {
      g_print("[E] could not create descriptor set \n");
      return -1;
    }

  VkWriteDescriptorSet wds[] = {
    {
      // binding 0, the uniform
      .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet          = desc_set,
      .descriptorCount = 1,
      .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .pBufferInfo     = &uni.descriptor,
      .dstBinding      = 0, // binding point
    },
    {
      .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet          = desc_set,
      .descriptorCount = 1,
      .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .dstBinding      = 1,

      .dstArrayElement = 0,
      .pImageInfo      = &(VkDescriptorImageInfo) {
	.imageLayout       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .imageView         = tex_image_view,
	.sampler           = tex_sampler,
      },
    }
  };

  vkUpdateDescriptorSets(dev, 2, wds, 0, NULL);

  g_print("ok \n");
  /* ************************************************************************ */
  g_print("   o-pipeline: ");

  VkPipelineInputAssemblyStateCreateInfo ias_ci = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    .primitiveRestartEnable = VK_FALSE,
  };

  VkPipelineRasterizationStateCreateInfo rasterstate_ci = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .polygonMode             = VK_POLYGON_MODE_FILL,
    .cullMode                = VK_CULL_MODE_NONE,
    .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    .depthClampEnable        = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .depthBiasEnable         = VK_FALSE,
    .lineWidth               = 1.0f,
  };

  VkPipelineColorBlendStateCreateInfo csb_ci = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .pNext = NULL,
    .attachmentCount = 1,
    .pAttachments    = (VkPipelineColorBlendAttachmentState[]) {
      {
	.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
	VK_COLOR_COMPONENT_G_BIT |
	VK_COLOR_COMPONENT_B_BIT |
	VK_COLOR_COMPONENT_A_BIT,
	.blendEnable    = VK_TRUE,
	.colorBlendOp = VK_BLEND_OP_ADD,
	.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
	.dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
	.alphaBlendOp   = VK_BLEND_OP_ADD,
	.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
	.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
      },
    },
    .logicOpEnable   = VK_FALSE,
    .blendConstants  = { 1.0f, 1.0f, 1.0f, 1.0f },
  };

  VkPipelineViewportStateCreateInfo vps_ci = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .scissorCount = 1,
  };

  VkPipelineDynamicStateCreateInfo dynstate_ci = {
    .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .pDynamicStates    = (VkDynamicState[]) {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR
    },
    .dynamicStateCount = 2,
  };

  VkPipelineDepthStencilStateCreateInfo depthss_ci = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .depthTestEnable = VK_FALSE, //was TRUE
    .depthWriteEnable = VK_FALSE, //was TRUE
    .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
    .depthBoundsTestEnable = VK_FALSE,
    .back.failOp = VK_STENCIL_OP_KEEP,
    .back.passOp = VK_STENCIL_OP_KEEP,
    .back.compareOp = VK_COMPARE_OP_ALWAYS,
    .stencilTestEnable = VK_FALSE,
  };

  depthss_ci.front = depthss_ci.back;

  VkPipelineMultisampleStateCreateInfo mss_ci = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    .pSampleMask = NULL,
  };

  VkPipelineVertexInputStateCreateInfo vertexis_ci = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount   = 1,
    .pVertexBindingDescriptions      = &(VkVertexInputBindingDescription) {
      .binding = 0,
      .stride = sizeof(float) * 2, // x, y
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    },
    .vertexAttributeDescriptionCount = 1,
    .pVertexAttributeDescriptions    = (VkVertexInputAttributeDescription[]) {
      {
	.binding  = 0,
	.location = 0,
	.format   = VK_FORMAT_R32G32_SFLOAT,
	.offset   = 0,
      },
    },
  };

  VkShaderModule vert_module = load_shader(dev, "box.vert.spv", &err);

  if (vert_module == VK_NULL_HANDLE)
    {
      g_print("[E] could not load shader: %s\n", err->message);
      return -1;
    }

  VkShaderModule frag_module = load_shader(dev, "color.frag.spv", &err);
  if (vert_module == VK_NULL_HANDLE)
    {
      g_print("[E] could not load shader: %s\n", err->message);
      return -1;
    }

  VkPipelineShaderStageCreateInfo shader_stages[] = {
    {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = vert_module,
      .pName = "main",
    },
    {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = frag_module,
      .pName = "main",
    }
  };

  VkGraphicsPipelineCreateInfo pipeline_ci = {
    .sType      = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .layout     = pipeline_layout,
    .renderPass = pass,
    .subpass    = 0,

    .stageCount = 2,
    .pStages = shader_stages,

    .pVertexInputState   = &vertexis_ci,
    .pInputAssemblyState = &ias_ci,
    .pRasterizationState = &rasterstate_ci,
    .pColorBlendState    = &csb_ci,
    .pMultisampleState   = &mss_ci,
    .pViewportState      = &vps_ci,
    .pDepthStencilState  = NULL,
    .pDynamicState       = &dynstate_ci,

    .basePipelineIndex   = -1,
    .basePipelineHandle  = VK_NULL_HANDLE,
  };

  VkPipeline pipeline;
  res = vkCreateGraphicsPipelines(dev,
				  pipeline_cache,
				  1,
				  &pipeline_ci,
				  NULL,
				  &pipeline);

  if (res != VK_SUCCESS)
    {
      g_print("[E] could not create pipeline\n");
      return -1;
    }

  vkDestroyShaderModule(dev, vert_module, NULL);
  vkDestroyShaderModule(dev, frag_module, NULL);

  g_print("ok \n");
  /* ************************************************************************ */
  g_print("   o-recording: ");

  VkCommandBufferBeginInfo cb_begin = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
  };

  VkClearValue cvs[1] = {
    {
      .color =  { { 30.0f / 256.0f,
		    30.0f / 256.0f,
		    30.0f / 256.0f,
		    0.0f} },
    }
  };

  VkRenderPassBeginInfo rpbi = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .pNext = NULL,
    .renderPass = pass,
    .renderArea.offset.x = 0,
    .renderArea.offset.y = 0,
    .renderArea.extent   = sc.currentExtent,
    .clearValueCount     = 1,
    .pClearValues        = cvs,
  };

  for (guint i = 0; i < imgs->len; i++)
    {
      g_print("[%u] ", i);

      VkFramebuffer fb = g_array_index(framebuffers, VkFramebuffer, i);
      rpbi.framebuffer = fb;

      res = vkBeginCommandBuffer(cmd_buf[i], &cb_begin);
      if (res != VK_SUCCESS)
        {
          g_print("[E] begin record .. \n");
          return -1;
        }

      vkg_transition_layout(cmd_buf[i],
			    tex_image,
			    VK_FORMAT_B8G8R8A8_UNORM,
			    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

      vkCmdCopyBufferToImage(cmd_buf[i],
			     tex_staging_buffer,
			     tex_image,
			     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			     1,
			     &tex_region);

      vkg_transition_layout(cmd_buf[i],
			    tex_image,
			    VK_FORMAT_B8G8R8A8_UNORM,
			    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);


      vkCmdBeginRenderPass(cmd_buf[i], &rpbi, VK_SUBPASS_CONTENTS_INLINE);

      VkViewport viewport = {
	.height   = sc.currentExtent.height,
	.width    = sc.currentExtent.width,
	.minDepth =  0.0f,
	.maxDepth =  1.0f,
      };

      vkCmdSetViewport(cmd_buf[i], 0, 1, &viewport);

      VkRect2D scissor = {
	.extent = sc.currentExtent,
	.offset.x = 0,
	.offset.y = 0,
      };

      vkCmdSetScissor(cmd_buf[i], 0, 1, &scissor);

      vkCmdBindDescriptorSets(cmd_buf[i],
			      VK_PIPELINE_BIND_POINT_GRAPHICS,
			      pipeline_layout,
			      0, 1,
			      &desc_set, 0,
			      NULL);

      vkCmdBindPipeline(cmd_buf[i],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipeline);

      VkDeviceSize offsets[1] = { 0 };
      vkCmdBindVertexBuffers(cmd_buf[i],
			     0, 1,
			     &vertex_area.device_buffer,
			     offsets);

      vkCmdBindIndexBuffer(cmd_buf[i],
			   index_area.device_buffer,
			   0,
			   VK_INDEX_TYPE_UINT32);

      vkCmdDrawIndexed(cmd_buf[i], 6, 1, 0, 0, 1);

      vkCmdEndRenderPass(cmd_buf[i]);

      res = vkEndCommandBuffer(cmd_buf[i]);
      if (res != VK_SUCCESS)
        {
          g_print("[E] end record\n");
          return -1;
        }
    }
  g_print(" ok \n");

  g_print(" o-rendering \n");
  gboolean suboptimal_have_warned = FALSE;

  g_print("   |- next image semaphore: ");
  VkSemaphore ni_sem = vkg_create_semaphore(dev, 0, &err);
  if (ni_sem == VK_NULL_HANDLE)
    {
      g_print("  [E] could not create semaphore %s", err->message);
      return -1;
    }
  g_print("ok\n");

  g_print("   |- render done semaphore: ");
  VkSemaphore rd_sem = vkg_create_semaphore(dev, 0, &err);
  if (rd_sem == VK_NULL_HANDLE)
    {
      g_print("  [E] could not create semaphore %s", err->message);
      return -1;
    }
  g_print("ok\n");

  while (!glfwWindowShouldClose(win))
    {
      glfwPollEvents();

      uint32_t iidx = 0;
      res = vkAcquireNextImageKHR(dev, swapchain, UINT64_MAX, ni_sem, NULL, &iidx);

      switch (res) {
      case VK_SUCCESS:
	break;

      case VK_SUBOPTIMAL_KHR:
	if (!suboptimal_have_warned)
	  g_print("   ! next-image: suboptimal warning!\n");
	suboptimal_have_warned = TRUE;
	break;


      case VK_ERROR_OUT_OF_DATE_KHR:
	  g_print("   ! next-image: out of date\n");
	break;

      default:
	g_print("   E next-image: unknown and unhandled error!\n");
	//TODO: close window
	break;
      }

      VkSubmitInfo si = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
	.waitSemaphoreCount   = 1,
	.pWaitSemaphores      = &ni_sem, // wait for this sem before processing
        .commandBufferCount   = 1,
        .pCommandBuffers      = &cmd_buf[iidx],
	.signalSemaphoreCount = 1,
	.pSignalSemaphores    = &rd_sem, // signale this sem when done
	.pWaitDstStageMask    = (VkPipelineStageFlags[]) {
	  VK_PIPELINE_STAGE_TRANSFER_BIT
	},
      };

      res = vkQueueSubmit(queue, 1, &si, NULL);
      if (res != VK_SUCCESS)
        {
          g_print("[E] queue submit\n");
          return -1;
        }

      VkPresentInfoKHR pi = {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .swapchainCount     = 1,
        .pSwapchains        = &swapchain,
        .pImageIndices      = &iidx,
	.waitSemaphoreCount = 1,
	.pWaitSemaphores    = &rd_sem,

      };

      res = vkQueuePresentKHR(queue, &pi);
      if (res != VK_SUCCESS)
        {
          g_print("[E] queue present\n");
          return -1;
        }

      vkDeviceWaitIdle(dev);

      ok = update_texture_with_clock(dev,
				     tex_staging_memory,
				     tex_mem_size,
				     tex_width, tex_height,
				     stride);
      if (!ok)
	{
	  g_print("[W] could not update the clock\n");
	}
  }

  vkDeviceWaitIdle(dev);


  vkDestroyPipeline(dev, pipeline, NULL);
  vkDestroyPipelineLayout(dev, pipeline_layout, NULL);
  vkDestroyPipelineCache(dev, pipeline_cache, NULL);

  vkDestroySampler(dev, tex_sampler, NULL);
  vkDestroyImageView(dev, tex_image_view, NULL);
  vkDestroyImage(dev, tex_image, NULL);
  vkFreeMemory(dev, tex_image_memory, NULL);
  vkDestroyBuffer(dev, tex_staging_buffer, NULL);
  vkFreeMemory(dev, tex_staging_memory, NULL);

  vkDestroyBuffer(dev, vertex_area.device_buffer, NULL);
  vkFreeMemory(dev, vertex_area.device_memory, NULL);
  vkDestroyBuffer(dev, index_area.device_buffer, NULL);
  vkFreeMemory(dev, index_area.device_memory, NULL);
  vkDestroyBuffer(dev, uni.buffer, NULL);
  vkFreeMemory(dev, uni.memory, NULL);
  for (guint i = 0; i < framebuffers->len; i++)
    {
      vkDestroyFramebuffer(dev, g_array_index(framebuffers, VkFramebuffer, i), NULL);
    }
  for (guint i = 0; i < views->len; i++)
    {
      vkDestroyImageView(dev, g_array_index(views, VkImageView, i), NULL);
    }

  vkDestroyDescriptorPool(dev, desc_pool, NULL);
  vkDestroyDescriptorSetLayout(dev, ds_layout, NULL);
  vkDestroyRenderPass(dev, pass, NULL);
  vkFreeCommandBuffers(dev, cp, sci_count, cmd_buf);
  g_free(cmd_buf);
  vkDestroyCommandPool(dev, cp, NULL);
  vkDestroySemaphore(dev, ni_sem, NULL);
  vkDestroySemaphore(dev, rd_sem, NULL);
  vkDestroySwapchainKHR(dev, swapchain, NULL);
  vkDestroyDevice(dev, NULL);
  vkDestroySurfaceKHR(inst, surface, NULL);
  vkDestroyInstance(inst, NULL);
  return 0;
}
