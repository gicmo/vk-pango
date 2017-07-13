
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
  VkCommandPool    cp;
  VkCommandBuffer *cmd_buf;

  GArray          *views; //VkImageView of SC images

  guint            render_id;
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
}


/* *** */
static int
vgk_swapchain_create_image_views(VkgWin  *win,
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

  VkCommandBufferBeginInfo begin_info = {
    VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    NULL,
    VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
    NULL
  };

  VkClearColorValue clear_color = {
    { 1.0f, 0.8f, 0.4f, 0.0f }
  };

  VkImageSubresourceRange range = {
    VK_IMAGE_ASPECT_COLOR_BIT,
    0,
    1,
    0,
    1
  };

  for (uint32_t i = 0; i <n_img; ++i) {
    VkImage img = gdk_vulkan_context_get_image(vk, i);

    VkImageMemoryBarrier p2w = {
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      NULL,
      VK_ACCESS_MEMORY_READ_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_QUEUE_FAMILY_IGNORED,
      VK_QUEUE_FAMILY_IGNORED,
      img,
      range
    };

    VkImageMemoryBarrier w2p = {
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      NULL,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_ACCESS_MEMORY_READ_BIT,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      VK_QUEUE_FAMILY_IGNORED,
      VK_QUEUE_FAMILY_IGNORED,
      img,
      range
    };

    vkBeginCommandBuffer(win->cmd_buf[i],
			 &begin_info);

    vkCmdPipelineBarrier(win->cmd_buf[i],
			 VK_PIPELINE_STAGE_TRANSFER_BIT,
			 VK_PIPELINE_STAGE_TRANSFER_BIT,
			 0, 0, NULL, 0, NULL, 1,
			 &p2w);

    vkCmdClearColorImage(win->cmd_buf[i],
			 img,
			 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			 &clear_color,
			 1,
			 &range);

    vkCmdPipelineBarrier(win->cmd_buf[i],
			 VK_PIPELINE_STAGE_TRANSFER_BIT,
			 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			 0, 0, NULL, 0, NULL, 1,
			 &w2p);

    res = vkEndCommandBuffer(win->cmd_buf[i]);

    if (res!= VK_SUCCESS)
      {
	g_set_error_literal(error, VULKAN_ERROR, (gint) res,
			    "could not record command buffers");
	return -1;
      }
  }

  return 0;
}

static void
images_updated_cb(GdkVulkanContext *vk,
		  VkgWin           *win)
{
  VkDevice dev = gdk_vulkan_context_get_device(vk);
  uint32_t n_img = gdk_vulkan_context_get_n_images(vk);
  g_autoptr(GError) err = NULL;
  uint32_t i;
  int      res;

  vkDeviceWaitIdle(dev);

  command_buffers_free(win);
  res = command_pool_reset(win, &err);

  if (res)
    g_error("[E] could not reset command pool: %s", err->message);

  win->sc_images = g_array_sized_new(FALSE,
				     FALSE,
				     sizeof(VkImage),
				     n_img);

  for (i = 0; i < n_img; i++)
    {
      VkImage img = gdk_vulkan_context_get_image(vk, i);
      g_array_append_val(win->sc_images, img);
    }

  res = command_buffers_create(win, &err);
  if (res)
    g_error("[E] command_buffers_create: %s", err->message);

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

  res = command_pool_create(win, &error);
  if (res)
    g_error("[E] command_pool_create: %s", error->message);

  images_updated_cb(win->vulkan, win);
  win->render_id = g_idle_add(vkg_win_render, win);
}

static void
vkg_win_unrealize(GtkWidget *widget)
{
  VkgWin *win = VKG_WIN(widget);

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
}

static gboolean
vkg_win_render (gpointer user_data)
{
  VkgWin *win = VKG_WIN(user_data);
  GdkVulkanContext *vk = win->vulkan;
  GdkDrawingContext *result;
  cairo_region_t *region;
  cairo_rectangle_int_t rect;
  GdkWindow *gdk_win;
  uint32_t draw_idx;
  VkResult res;
  VkQueue queue;
  VkSemaphore isem;

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
