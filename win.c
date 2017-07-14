
#include <vulkan/vulkan.h>

#include <graphene.h>

#include <glib.h>
#include <string.h>
#include <cairo.h>

#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

typedef struct VkGStagingArea_ {
  VkDeviceMemory staging_memory;
  VkBuffer       staging_buffer;

  VkDeviceMemory device_memory;
  VkBuffer       device_buffer;
} VkGStagingArea;


GQuark
vulkan_error_quark (void)
{
  return g_quark_from_static_string ("vulkan-error-quark");
}
#define VULKAN_ERROR vulkan_error_quark ()

#define VKG_TYPE_WIN (vkg_win_get_type())
G_DECLARE_FINAL_TYPE(VkgWin, vkg_win, VKG, WIN, GtkWindow);

enum {
  //CLOSE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct _VkgWin
{
  GtkWindow	parent_instance;

  GdkVulkanContext *vulkan;

  GArray          *sc_images;
  VkExtent2D       sc_extent;
  VkCommandPool    cp;
  VkCommandBuffer *cmd_buf;

  VkRenderPass pass;
  VkPipelineCache pipeline_cache;

  GArray          *views; //VkImageView of SC images
  GArray          *framebuffers;
  guint            render_id;

  VkGStagingArea vertex_area;
  VkGStagingArea index_area;

  VkDeviceMemory tex_staging_memory;
  VkBuffer       tex_staging_buffer;

  VkBufferImageCopy tex_region;
  VkImage tex_image;
  VkDeviceMemory tex_image_memory;
  VkImageView tex_image_view;
  VkSampler tex_sampler;
  int tex_size;
  int tex_stride;
  VkDeviceSize tex_mem_size;

  VkDescriptorSetLayout ds_layout;
  VkDescriptorPool desc_pool;

  VkPipelineLayout pipeline_layout;
  VkDescriptorSet desc_set;
  VkPipeline pipeline;

  Uni uni;
  float               zoom;
  graphene_point3d_t  rotation;
};

G_DEFINE_TYPE(VkgWin, vkg_win, GTK_TYPE_WINDOW);

static void vkg_win_realize   (GtkWidget *widget);
static void vkg_win_unrealize (GtkWidget *widget);

static void images_updated_cb (GdkVulkanContext *context,
			       VkgWin           *win);

static gboolean vkg_win_render (gpointer user_data);

static void
vkg_win_class_init (VkgWinClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

  widget_class->realize = vkg_win_realize;
  widget_class->unrealize = vkg_win_unrealize;
}

static void
vkg_win_init (VkgWin *win)
{
  gtk_window_set_screen(GTK_WINDOW(win), gtk_widget_get_screen(GTK_WIDGET(win)));
  gtk_window_set_title(GTK_WINDOW(win), "Vulkan Test");
  gtk_window_set_default_size(GTK_WINDOW(win), 800, 600);

  win->zoom = -2.5f;
  win->rotation = GRAPHENE_POINT3D_INIT(0.f, 0.f, 0.f);
}

/* +++++++++++++++++++ */

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

/* +++++++++++++++++++++++++++++++++++ */

/* *** */
static int
sc_image_views_create(VkgWin  *win,
		      GError  **error)
{
  GdkVulkanContext *vk = win->vulkan;
  uint32_t n_img = gdk_vulkan_context_get_n_images(vk);
  VkFormat format = gdk_vulkan_context_get_image_format(vk);
  VkDevice dev = gdk_vulkan_context_get_device(vk);
  VkResult res;
  GArray   *views;

  views = g_array_sized_new(FALSE, FALSE, sizeof(VkImageView), n_img);
  //TODO: free func?

  for (guint i = 0; i < n_img; i++)
    {
      VkImage img = gdk_vulkan_context_get_image(vk, i);
      VkImageView *view = &g_array_index(views, VkImageView, i);

      VkImageViewCreateInfo ci = {
	.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
	.pNext                           = NULL,
	.format                          = format,
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
	.image                           = img,
      };

      res = vkCreateImageView(dev, &ci, NULL, view);

      if (res != VK_SUCCESS)
	{
	  g_array_free(views, TRUE);
	  g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			      "failed");
	  return -1;
	}

      views->len = i + 1;
    }

  win->views = views;
  return 0;
}

static int
render_pass_create(VkgWin  *win,
		   GError **error)
{
  GdkVulkanContext *vk = win->vulkan;
  VkFormat format = gdk_vulkan_context_get_image_format(vk);
  VkDevice dev = gdk_vulkan_context_get_device(vk);
  VkFormat color_format = format;
  VkFormat depth_format = format;
  VkResult res;

  g_print("   o-render pass: ");

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


  res = vkCreateRenderPass(dev, &rp_info, NULL, &win->pass);
  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "failed");
      return -1;
    }

  g_print("ok \n");
  return 0;
}


static int
pipeline_cache_create(VkgWin  *win,
		      GError  **error)
{
  GdkVulkanContext *vk = win->vulkan;
  VkDevice dev = gdk_vulkan_context_get_device(vk);
  VkResult res;

  g_print("   o-pipeline cache: ");

  VkPipelineCacheCreateInfo pc_ci = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
  };

  res = vkCreatePipelineCache(dev, &pc_ci, NULL, &win->pipeline_cache);
  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "could not create pipeline cache");
      return -1;
    }

  g_print("ok \n");
  return 0;
}

static int
sc_framebuffers_create(VkgWin  *win,
		       GError  **error)
{
  GdkVulkanContext *vk = win->vulkan;
  VkDevice dev = gdk_vulkan_context_get_device(vk);
  GArray *views = win->views;
  GArray *framebuffers;
  GdkWindow *gdk_win;
  VkResult res;

  gdk_win = gtk_widget_get_window(GTK_WIDGET(win));

  g_print("   o-framebuffer: ");

  framebuffers = g_array_sized_new(FALSE,
				   FALSE,
				   sizeof(VkFramebuffer),
				   views->len);

  for (guint i = 0; i < views->len; i++)
    {
      g_print("[%d] ", i);

      VkFramebuffer *fb = &g_array_index(framebuffers, VkFramebuffer, i);

      VkFramebufferCreateInfo fb_ci = {
	.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
	.pNext           = NULL,
	.renderPass      = win->pass,
	.attachmentCount = 1,
	.pAttachments    = &g_array_index(views, VkImageView, i),
	.width           = gdk_window_get_width(gdk_win),
	.height          = gdk_window_get_height(gdk_win),
	.layers          = 1,
      };

      res = vkCreateFramebuffer(dev, &fb_ci, NULL, fb);
      if (res != VK_SUCCESS)
	{
	  g_array_free(framebuffers, TRUE);
	  framebuffers = NULL;
	  g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			      "could not create frame buffer");
	  break;
	}

      framebuffers->len = i + 1;
    }
  if (framebuffers == NULL)
    {
      return -1;
    }

  win->framebuffers = framebuffers;
  g_print("ok \n");
  return 0;
}

static int
vertex_data_create(VkgWin *win,
		   GError **err)
{
  GdkVulkanContext *vk = win->vulkan;
  VkDevice dev = gdk_vulkan_context_get_device(vk);
  VkPhysicalDevice phy = gdk_vulkan_context_get_physical_device(vk);
  VkQueue queue = gdk_vulkan_context_get_queue(vk);
  VkPhysicalDeviceMemoryProperties dev_mem_props;
  gboolean ok;

  g_print("   o-vertex data: ");
  vkGetPhysicalDeviceMemoryProperties(phy, &dev_mem_props);

  float box[] = { -0.5f,  0.5f,
		  -0.5f, -0.5f,
		  0.5f, -0.5f,

		   0.5f,  0.5f,
		  -0.5f,  0.5f,
		   0.5f, -0.5f};

  uint32_t box_idx[] = {0, 1, 2, 3, 4, 5};

  ok = vkg_memcpy_stage(dev,
			phy,
			box,
			sizeof(box),
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			&win->vertex_area, err);

  if (!ok)
    {
      return -1;
    }

  ok = vkg_memcpy_stage(dev,
			phy,
			box_idx,
			sizeof(box_idx),
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			&win->index_area, err);

  if (!ok)
    {
      return -1;
    }

  VkCommandBuffer copy_cmd = vkg_command_buffer_get(dev, win->cp, TRUE, err);
  if (copy_cmd == VK_NULL_HANDLE)
    {
      return -1;
    }

  VkBufferCopy copy_region = {
    .size = sizeof(box),
  };

  vkCmdCopyBuffer(copy_cmd,
		  win->vertex_area.staging_buffer,
		  win->vertex_area.device_buffer,
		  1,
		  &copy_region);

  copy_region.size = sizeof(box_idx);

  vkCmdCopyBuffer(copy_cmd,
		  win->index_area.staging_buffer,
		  win->index_area.device_buffer,
		  1,
		  &copy_region);

  ok = vkg_command_buffer_flush(dev, queue, win->cp, copy_cmd, err);

  if (!ok)
    {
      return -1;
    }

  vkDestroyBuffer(dev, win->vertex_area.staging_buffer, NULL);
  vkFreeMemory(dev, win->vertex_area.staging_memory, NULL);
  vkDestroyBuffer(dev, win->index_area.staging_buffer, NULL);
  vkFreeMemory(dev, win->index_area.staging_memory, NULL);

  g_print("ok \n");
  return 0;
}

static int
texture_create(VkgWin *win,
	       GError **error)
{
  GdkVulkanContext *vk = win->vulkan;
  VkDevice dev = gdk_vulkan_context_get_device(vk);
  VkPhysicalDevice phy = gdk_vulkan_context_get_physical_device(vk);
  VkQueue queue = gdk_vulkan_context_get_queue(vk);
  VkResult res;
  VkPhysicalDeviceMemoryProperties dev_mem_props;
  gboolean ok;
  g_autoptr(GError) err = NULL;

  vkGetPhysicalDeviceMemoryProperties(phy, &dev_mem_props);

  size_t tex_size = 1024;
  int tex_height = tex_size, tex_width = tex_size;
  int stride = -1;

  g_print("     o-size: %lu \n", tex_size);
  stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, tex_width);
  win->tex_mem_size = tex_height * stride;

  g_print("     o-stride: %i \n", stride);
  g_print("     o-mem size: %lu \n", win->tex_mem_size);

  VkMemoryRequirements tex_mreq = { };

  VkBufferCreateInfo tex_bci = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size  = win->tex_mem_size,
    .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };

  res = vkCreateBuffer(dev, &tex_bci, NULL, &win->tex_staging_buffer);

  if (res != VK_SUCCESS)
    {
      g_print("staging buffer creation failed");
      return -1;
    }

  vkGetBufferMemoryRequirements(dev, win->tex_staging_buffer, &tex_mreq);

  VkMemoryAllocateInfo tex_ai = {
    .sType          = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = tex_mreq.size,
  };

  ok = vkg_auto_mem_type_index(&tex_mreq,
			       dev_mem_props,
			       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			       &tex_ai);

  res = vkAllocateMemory(dev, &tex_ai, NULL, &win->tex_staging_memory);

  if (res != VK_SUCCESS)
    {
      g_print("staging buffer memory allocation failed");
      return -1;
    }

  res = vkBindBufferMemory(dev,
			   win->tex_staging_buffer,
			   win->tex_staging_memory,
			   0);

  if (res != VK_SUCCESS)
    {
      g_print("staging buffer binding failed");
      return -1;
    }


  /* initial texture transfer */
  ok = update_texture_with_clock(dev,
				 win->tex_staging_memory,
				 win->tex_mem_size,
				 tex_width, tex_height,
				 stride);

  if (!ok)
    {
      g_print("could not draw to the clock");
      return -1;
    }

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

  res = vkCreateImage(dev, &tex_ici, NULL, &win->tex_image);

  if (res != VK_SUCCESS)
    {
      g_print("tex image creation failed\n");
      return -1;
    }

  vkGetImageMemoryRequirements(dev, win->tex_image, &tex_mreq);

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

  res = vkAllocateMemory(dev, &tex_ai, NULL, &win->tex_image_memory);
  if (res != VK_SUCCESS)
    {
      g_print("could not allocation memory for texture\n");
      return -1;
    }

  vkBindImageMemory(dev,
		    win->tex_image,
		    win->tex_image_memory, 0);

  VkCommandBuffer copy_cmd = vkg_command_buffer_get(dev, win->cp, TRUE, &err);
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

  win->tex_region = tex_region;
  vkg_transition_layout(copy_cmd,
			win->tex_image,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_LAYOUT_PREINITIALIZED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  vkCmdCopyBufferToImage(copy_cmd,
			 win->tex_staging_buffer,
			 win->tex_image,
			 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			 1,
			 &tex_region);

  vkg_transition_layout(copy_cmd,
			win->tex_image,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  ok = vkg_command_buffer_flush(dev, queue, win->cp, copy_cmd, &err);

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
    .image                           = win->tex_image,
  };

  res = vkCreateImageView(dev, &tex_ci, NULL, &win->tex_image_view);

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

  res = vkCreateSampler(dev, &tex_sci, NULL, &win->tex_sampler);

  if (res != VK_SUCCESS)
    {
      g_print("[E] could not create sampler\n");
      return -1;
    }

  g_print("     ok \n");

  win->tex_size = tex_size;
  win->tex_stride = stride;

  return 0;
}

static int
uniform_create(VkgWin *win)
{
  GdkVulkanContext *vk = win->vulkan;
  VkDevice dev = gdk_vulkan_context_get_device(vk);
  VkResult res;
  VkPhysicalDeviceMemoryProperties dev_mem_props;
  gboolean ok;
  g_autoptr(GError) err = NULL;

  /* ************************************************************************ */
  g_print("   o-uniform: ");

  win->uni.size = sizeof(float) * 16 * 3;

  VkBufferCreateInfo uni_bci = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = win->uni.size, // 4x4 matrix, 3 times
    .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
  };

  res = vkCreateBuffer(dev, &uni_bci, NULL, &win->uni.buffer);

  VkMemoryRequirements uni_mreq;
  vkGetBufferMemoryRequirements(dev, win->uni.buffer, &uni_mreq);

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

  res = vkAllocateMemory(dev, &uni_ai, NULL, &win->uni.memory);
  if (res != VK_SUCCESS)
    {
      g_print("[E] could not allocate memory\n");
      return -1;
    }

  res = vkBindBufferMemory(dev, win->uni.buffer, win->uni.memory, 0);
  if (res != VK_SUCCESS)
    {
      g_print("[E] could not bind memory\n");
      return -1;
    }

  win->uni.descriptor.buffer = win->uni.buffer;
  win->uni.descriptor.offset = 0;
  win->uni.descriptor.range = win->uni.size;

  ok = update_uni_data(dev,
		       &win->uni,
		       win->sc_extent,
		       win->zoom,
		       win->rotation,
		       &err);

  if (!ok)
    {
      g_print("[E] updating uniform: %s\n", err->message);
    }

  g_print("ok \n");
  return 0;
}

/* ***  */

static int
pipeline_create(VkgWin  *win,
		GError **error)
{
  GdkVulkanContext *vk = win->vulkan;
  VkDevice dev = gdk_vulkan_context_get_device(vk);
  VkResult res;
  g_autoptr(GError) err = NULL;

  g_print("   o-pipeline layout: ");

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


  res = vkCreateDescriptorSetLayout(dev, &layout_desc, NULL, &win->ds_layout);
  if (res != VK_SUCCESS)
    {
      g_print("[E] could not create descriptor set layout\n");
      return -1;
    }

  VkPipelineLayoutCreateInfo pl_ci = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .pNext = NULL,
    .setLayoutCount = 1,
    .pSetLayouts = &win->ds_layout,
  };


  res = vkCreatePipelineLayout(dev, &pl_ci, NULL, &win->pipeline_layout);
  if (res != VK_SUCCESS)
    {
      g_print("[E] could not create pipeline layout\n");
      return -1;
    }

  g_print("ok \n");

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


  res = vkCreateDescriptorPool(dev, &dsp_ci, NULL, &win->desc_pool);
  if (res != VK_SUCCESS)
    {
      g_print("[E] could not create pipeline layout\n");
      return -1;
    }

  g_print("ok \n");

  g_print("   o-descriptor set: ");

  VkDescriptorSetAllocateInfo ds_ai = {
    .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool     = win->desc_pool,
    .descriptorSetCount = 1,
    .pSetLayouts        = &win->ds_layout,
  };


  res = vkAllocateDescriptorSets(dev, &ds_ai, &win->desc_set);
  if (res != VK_SUCCESS)
    {
      g_print("[E] could not create descriptor set \n");
      return -1;
    }

  VkWriteDescriptorSet wds[] = {
    {
      // binding 0, the uniform
      .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet          = win->desc_set,
      .descriptorCount = 1,
      .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .pBufferInfo     = &win->uni.descriptor,
      .dstBinding      = 0, // binding point
    },
    {
      .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet          = win->desc_set,
      .descriptorCount = 1,
      .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .dstBinding      = 1,

      .dstArrayElement = 0,
      .pImageInfo      = &(VkDescriptorImageInfo) {
	.imageLayout       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .imageView         = win->tex_image_view,
	.sampler           = win->tex_sampler,
      },
    }
  };

  vkUpdateDescriptorSets(dev, 2, wds, 0, NULL);

  g_print("ok \n");
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
    .layout     = win->pipeline_layout,
    .renderPass = win->pass,
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


  res = vkCreateGraphicsPipelines(dev,
				  win->pipeline_cache,
				  1,
				  &pipeline_ci,
				  NULL,
				  &win->pipeline);

  if (res != VK_SUCCESS)
    {
      g_print("[E] could not create pipeline\n");
      return -1;
    }

  vkDestroyShaderModule(dev, vert_module, NULL);
  vkDestroyShaderModule(dev, frag_module, NULL);

  g_print("ok \n");

  return 0;
}

/* *** */

static int
command_pool_create(VkgWin  *win,
		    GError **error)
{
  GdkVulkanContext *vk = win->vulkan;
  uint32_t qf_idx = gdk_vulkan_context_get_queue_family_index(vk);
  VkDevice dev = gdk_vulkan_context_get_device(vk);
  VkResult res;

  g_print("   o-command pool: ");

  VkCommandPoolCreateInfo cpc_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .queueFamilyIndex = qf_idx,
  };


  VkCommandPool cp;
  res = vkCreateCommandPool(dev, &cpc_info, NULL, &cp);
  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "could not create command pool");
      win->cp = VK_NULL_HANDLE;
      return -1;
    }

  win->cp = cp;
  g_print("ok \n");
  return 0;
}

static int
command_pool_reset(VkgWin  *win,
		   GError **error)
{
  VkDevice dev = gdk_vulkan_context_get_device (win->vulkan);
  VkResult res;

  res = vkResetCommandPool(dev, win->cp, 0);

  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "could not reset command pool");
      win->cp = VK_NULL_HANDLE;
      return -1;
    }

  return 0;
}

static void
command_pool_free(VkgWin *win)
{
    VkDevice dev = gdk_vulkan_context_get_device (win->vulkan);
    vkDestroyCommandPool(dev, win->cp, NULL);
}

static int
command_buffers_create(VkgWin  *win,
		       GError **error)
{
  GdkVulkanContext *vk = win->vulkan;
  uint32_t n_img = gdk_vulkan_context_get_n_images(vk);
  VkDevice dev = gdk_vulkan_context_get_device(vk);
  VkResult res;

  g_print("   o-command buffers: ");

  VkCommandBufferAllocateInfo cba_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = win->cp,
    .commandBufferCount = n_img,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
  };

  VkCommandBuffer *cmd_buf = g_new0(VkCommandBuffer, n_img);
  res = vkAllocateCommandBuffers(dev, &cba_info, cmd_buf);
  if (res != VK_SUCCESS)
    {
      g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			  "could not create command buffers");
      return -1;
    }

  win->cmd_buf = cmd_buf;
  g_print("ok \n");
  return 0;
}

static void
command_buffers_free(VkgWin  *win)
{
  GdkVulkanContext *vk = win->vulkan;
  uint32_t n_img = gdk_vulkan_context_get_n_images(vk);
  VkDevice dev = gdk_vulkan_context_get_device(vk);

  if (win->cmd_buf == NULL)
    return;

  vkFreeCommandBuffers(dev, win->cp, n_img, win->cmd_buf);
  win->cmd_buf = NULL;

}

static int
command_buffers_record(VkgWin  *win,
		       GError **error)
{
  GdkVulkanContext *vk = win->vulkan;
  uint32_t n_img = gdk_vulkan_context_get_n_images(vk);
  VkResult res;

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
    .renderPass = win->pass,
    .renderArea.offset.x = 0,
    .renderArea.offset.y = 0,
    .renderArea.extent   = win->sc_extent,
    .clearValueCount     = 1,
    .pClearValues        = cvs,
  };

  for (guint i = 0; i < n_img; i++)
    {
      g_print("[%u] ", i);

      VkFramebuffer fb = g_array_index(win->framebuffers, VkFramebuffer, i);
      rpbi.framebuffer = fb;

      res = vkBeginCommandBuffer(win->cmd_buf[i], &cb_begin);
      if (res != VK_SUCCESS)
        {
          g_print("[E] begin record .. \n");
          return -1;
        }

      vkg_transition_layout(win->cmd_buf[i],
			    win->tex_image,
			    VK_FORMAT_B8G8R8A8_UNORM,
			    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

      vkCmdCopyBufferToImage(win->cmd_buf[i],
			     win->tex_staging_buffer,
			     win->tex_image,
			     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			     1,
			     &win->tex_region);

      vkg_transition_layout(win->cmd_buf[i],
			    win->tex_image,
			    VK_FORMAT_B8G8R8A8_UNORM,
			    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

      vkCmdBeginRenderPass(win->cmd_buf[i], &rpbi, VK_SUBPASS_CONTENTS_INLINE);

      VkViewport viewport = {
	.height   = win->sc_extent.height,
	.width    = win->sc_extent.width,
	.minDepth = 0.0f,
	.maxDepth = 1.0f,
      };

      vkCmdSetViewport(win->cmd_buf[i], 0, 1, &viewport);

      VkRect2D scissor = {
	.extent = win->sc_extent,
	.offset.x = 0,
	.offset.y = 0,
      };

      vkCmdSetScissor(win->cmd_buf[i], 0, 1, &scissor);

      vkCmdBindDescriptorSets(win->cmd_buf[i],
			      VK_PIPELINE_BIND_POINT_GRAPHICS,
			      win->pipeline_layout,
			      0, 1,
			      &win->desc_set, 0,
			      NULL);

      vkCmdBindPipeline(win->cmd_buf[i],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			win->pipeline);

      VkDeviceSize offsets[1] = { 0 };
      vkCmdBindVertexBuffers(win->cmd_buf[i],
			     0, 1,
			     &win->vertex_area.device_buffer,
			     offsets);

      vkCmdBindIndexBuffer(win->cmd_buf[i],
			   win->index_area.device_buffer,
			   0,
			   VK_INDEX_TYPE_UINT32);

      vkCmdDrawIndexed(win->cmd_buf[i], 6, 1, 0, 0, 1);

      vkCmdEndRenderPass(win->cmd_buf[i]);

      res = vkEndCommandBuffer(win->cmd_buf[i]);
      if (res != VK_SUCCESS)
        {
          g_print("[E] end record\n");
          return -1;
        }
    }
  g_print(" ok \n");
  return 0;
}

static void
images_updated_cb(GdkVulkanContext *vk,
		  VkgWin           *win)
{
  VkDevice dev = gdk_vulkan_context_get_device(vk);
  uint32_t n_img = gdk_vulkan_context_get_n_images(vk);
  GdkWindow *gdk_win;
  g_autoptr(GError) err = NULL;
  uint32_t i;
  int      res;
  gboolean ok;

  vkDeviceWaitIdle(dev);

  command_buffers_free(win);
  res = command_pool_reset(win, &err);
  if (res)
    g_error("[E] could not reset command pool: %s", err->message);

  if (win->pass)
    {
      vkDestroyRenderPass(dev, win->pass, NULL);

      vkDestroyDescriptorSetLayout(dev, win->ds_layout, NULL);
      vkDestroyDescriptorPool(dev, win->desc_pool, NULL);

      vkDestroyPipelineLayout(dev, win->pipeline_layout, NULL);
      vkDestroyPipelineCache(dev, win->pipeline_cache, NULL);
      vkDestroyPipeline(dev, win->pipeline, NULL);


      for (guint i = 0; i < win->framebuffers->len; i++)
	{
	  VkFramebuffer fb = g_array_index(win->framebuffers, VkFramebuffer, i);
	  vkDestroyFramebuffer(dev, fb, NULL);
	}

      for (guint i = 0; i < win->views->len; i++)
	{
	  VkImageView view = g_array_index(win->views, VkImageView, i);
	  vkDestroyImageView(dev, view, NULL);
	}


      g_array_remove_range(win->sc_images, 0, win->sc_images->len);
    }
  /* *** clearing done *** */

  ok = update_uni_data(dev,
		       &win->uni,
		       win->sc_extent,
		       win->zoom,
		       win->rotation,
		       &err);

  if (!ok)
    {
      g_print("[E] updating uniform: %s\n", err->message);
    }

  win->sc_images = g_array_sized_new(FALSE,
				     FALSE,
				     sizeof(VkImage),
				     n_img);

  for (i = 0; i < n_img; i++)
    {
      VkImage img = gdk_vulkan_context_get_image(vk, i);
      g_array_append_val(win->sc_images, img);
    }

  gdk_win = gtk_widget_get_window(GTK_WIDGET(win));

  win->sc_extent.width = gdk_window_get_width(gdk_win);
  win->sc_extent.height = gdk_window_get_height(gdk_win);

  res = command_buffers_create(win, &err);
  if (res)
    g_error("[E] command_buffers_create: %s", err->message);

  res = render_pass_create(win, &err);
  if (res)
    g_error("[E] render pass: %s", err->message);

  res = pipeline_cache_create(win, &err);
  if (res)
    g_error("[E] pipeline cache: %s", err->message);

  res = sc_image_views_create(win, &err);
  if (res)
    g_error("[E] image views: %s", err->message);

  res = sc_framebuffers_create(win, &err);
  if (res)
    g_error("[E] framebuffer: %s", err->message);

  res = pipeline_create(win, &err);
  if (res)
    g_error("[E] pipeline creation: %s", err->message);

  res = command_buffers_record(win, &err);
  if (res)
    g_error("[E] record_command_buffers: %s", err->message);

}

/* **** */

static void
vkg_win_realize (GtkWidget *widget)
{
  VkgWin *win = VKG_WIN(widget);
  GdkVulkanContext *vk_ctx;
  GdkWindow *gdk_win;
  int res;
  g_autoptr(GError) error = NULL;

  GTK_WIDGET_CLASS(vkg_win_parent_class)->realize(widget);

  gdk_win = gtk_widget_get_window(widget);
  vk_ctx = gdk_window_create_vulkan_context(gdk_win, &error);

  if (vk_ctx == NULL)
    {
      g_error("Could not get vulkan context %s", error->message);
    }

  g_debug("Got a vulkan context!");
  win->vulkan = vk_ctx;

  g_signal_connect(win->vulkan,
                   "images-updated",
                   G_CALLBACK (images_updated_cb),
                   win);

  win->sc_extent.width = gdk_window_get_width(gdk_win);
  win->sc_extent.height = gdk_window_get_height(gdk_win);

  res = command_pool_create(win, &error);
  if (res)
    g_error("[E] command_pool_create: %s", error->message);

  res = vertex_data_create(win, &error);
  if (res)
    g_error("[E] vertex data: %s", error->message);

  res = texture_create(win, &error);
  if (res)
    g_error("[E] texture: %s", error->message);

  res = uniform_create(win);
  if (res)
    g_error("[E] uniform: %s", error->message);

  images_updated_cb(win->vulkan, win);
  win->render_id = g_idle_add(vkg_win_render, win);
}

static void
vkg_win_unrealize(GtkWidget *widget)
{
  VkgWin *win = VKG_WIN(widget);
  VkDevice dev = gdk_vulkan_context_get_device(win->vulkan);

  GTK_WIDGET_CLASS(vkg_win_parent_class)->unrealize(widget);

  if (win->render_id)
    {
      g_source_remove(win->render_id);
      win->render_id = 0;
    }

  g_signal_handlers_disconnect_by_func(win->vulkan,
                                       images_updated_cb,
                                       win);

  command_buffers_free(win);
  command_pool_free(win);

  vkDestroySampler(dev, win->tex_sampler, NULL);
  vkDestroyImageView(dev, win->tex_image_view, NULL);
  vkDestroyImage(dev, win->tex_image, NULL);
  vkFreeMemory(dev, win->tex_image_memory, NULL);
  vkDestroyBuffer(dev, win->tex_staging_buffer, NULL);
  vkFreeMemory(dev, win->tex_staging_memory, NULL);

  vkDestroyBuffer(dev, win->vertex_area.device_buffer, NULL);
  vkFreeMemory(dev, win->vertex_area.device_memory, NULL);
  vkDestroyBuffer(dev, win->index_area.device_buffer, NULL);
  vkFreeMemory(dev, win->index_area.device_memory, NULL);
  vkDestroyBuffer(dev, win->uni.buffer, NULL);
  vkFreeMemory(dev, win->uni.memory, NULL);

}

static gboolean
vkg_win_render (gpointer user_data)
{
  VkgWin *win = VKG_WIN(user_data);
  GdkVulkanContext *vk = win->vulkan;
  VkDevice dev = gdk_vulkan_context_get_device(vk);
  GdkDrawingContext *result;
  cairo_region_t *region;
  cairo_rectangle_int_t rect;
  GdkWindow *gdk_win;
  uint32_t draw_idx;
  VkResult res;
  VkQueue queue;
  VkSemaphore isem;
  gboolean ok;

  gdk_win = gtk_widget_get_window(GTK_WIDGET(win));

  rect.x = rect.y = 0;
  rect.width =  gdk_window_get_width(gdk_win);
  rect.height = gdk_window_get_height(gdk_win);

  region = cairo_region_create_rectangle(&rect);
  result = gdk_window_begin_draw_frame(gdk_win,
				       GDK_DRAW_CONTEXT(vk),
				       region);

  draw_idx = gdk_vulkan_context_get_draw_index(vk);
  queue = gdk_vulkan_context_get_queue(vk);
  isem = gdk_vulkan_context_get_draw_semaphore(vk);

  VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
  VkSubmitInfo submit_info = {
    VK_STRUCTURE_TYPE_SUBMIT_INFO,
    NULL,
    1,
    &isem,
    &wait_dst_stage_mask,
    1,
    &win->cmd_buf[draw_idx],
    0,
    NULL
  };

  res = vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);

  if (res != VK_SUCCESS) {
    g_error("Could not submit queue");
  }

  gdk_window_end_draw_frame(gdk_win, result);
  cairo_region_destroy(region);

  vkDeviceWaitIdle(dev);

  ok = update_texture_with_clock(dev,
				 win->tex_staging_memory,
				 win->tex_mem_size,
				 win->tex_size, win->tex_size,
				 win->tex_stride);
  if (!ok)
    {
      g_print("[W] could not update the clock\n");
    }

  return TRUE;
}

/* *** */
int
main(int argc, char **argv)
{
  GtkWidget* win;

  gtk_init();

  win = g_object_new(VKG_TYPE_WIN, NULL) ;

  gtk_widget_show(win);
  gtk_main();

  return 0;
}
