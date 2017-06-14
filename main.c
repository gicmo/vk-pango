#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

#include <glib.h>

#define VULKAN_ERROR vulkan_error_quark ()

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

int main(int argc, char **argv)
{
  gboolean enable_validation = TRUE;
  VkResult res;
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


  g_print("     o-recording: ");
  for (guint i = 0; i < imgs->len; i++)
    {
      VkImage img = g_array_index(imgs, VkImage, i);
      g_print("[%u] ", i);

      VkImageSubresourceRange sr_range = {
	.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	.levelCount = 1,
	.layerCount = 1,
      };

      /* barriers */

      VkImageMemoryBarrier from_present = {
	.sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
	.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
	.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
	.oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED,
	.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
	.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED,
	.image = img,
	.subresourceRange = sr_range,
      };

      VkImageMemoryBarrier to_present = {
	.sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
	.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
	.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
	.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
	.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
	.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED,
	.image = img,
	.subresourceRange = sr_range,
      };
      /* recording */
      VkCommandBufferBeginInfo cb_begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
      };

      VkClearColorValue cv  = { 164.0f/256.0f, 30.0f/256.0f, 34.0f/256.0f, 0.0f };

      res = vkBeginCommandBuffer(cmd_buf[i], &cb_begin);
      if (res != VK_SUCCESS)
        {
          g_print("[E] begin record .. \n");
          return -1;
        }

      vkCmdPipelineBarrier(cmd_buf[i],
			   VK_PIPELINE_STAGE_TRANSFER_BIT, // source stage mask
			   VK_PIPELINE_STAGE_TRANSFER_BIT, // dest stage mask
			   0,                              //depdencyFlags
			   0, NULL,                        //memory barriers
			   0, NULL,                        //buffer barriers
			   1, &from_present);              //image barriers

      vkCmdClearColorImage(cmd_buf[i], img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &cv, 1, &sr_range);


      vkCmdPipelineBarrier(cmd_buf[i],
			   VK_PIPELINE_STAGE_TRANSFER_BIT,
			   VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			   0,
			   0, NULL,
			   0, NULL,
			   1, &to_present);

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
  }

  vkDeviceWaitIdle(dev);
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
