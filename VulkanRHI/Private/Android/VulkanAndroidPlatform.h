// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHI.h"

#define VK_USE_PLATFORM_ANDROID_KHR					1

#define VULKAN_ENABLE_DUMP_LAYER					0
#define VULKAN_DYNAMICALLYLOADED					1
#define VULKAN_SHOULD_ENABLE_DRAW_MARKERS			(UE_BUILD_DEVELOPMENT || UE_BUILD_DEBUG)
#define VULKAN_USE_IMAGE_ACQUIRE_FENCES				0
#define VULKAN_USE_CREATE_ANDROID_SURFACE			1
#define VULKAN_SHOULD_USE_LLM						(UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT)
#define VULKAN_SHOULD_USE_COMMANDWRAPPERS			VULKAN_SHOULD_USE_LLM //LLM on Vulkan needs command wrappers to account for vkallocs
#define VULKAN_ENABLE_LRU_CACHE						1
#define VULKAN_SUPPORTS_GOOGLE_DISPLAY_TIMING		1
#define VULKAN_FREEPAGE_FOR_TYPE					1
#define VULKAN_PURGE_SHADER_MODULES					0
#define VULKAN_SUPPORTS_DEDICATED_ALLOCATION		0
#define VULKAN_SUPPORTS_PHYSICAL_DEVICE_PROPERTIES2	0


// Android's hashes currently work fine as the problematic cases are:
//	VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL = 1000117000,
//	VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL = 1000117001,
#define VULKAN_USE_REAL_RENDERPASS_COMPATIBILITY	0


#define ENUM_VK_ENTRYPOINTS_PLATFORM_BASE(EnumMacro)

#define ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE(EnumMacro) \
	EnumMacro(PFN_vkCreateAndroidSurfaceKHR, vkCreateAndroidSurfaceKHR) \

#define ENUM_VK_ENTRYPOINTS_OPTIONAL_PLATFORM_INSTANCE(EnumMacro) \
	EnumMacro(PFN_vkGetRefreshCycleDurationGOOGLE, vkGetRefreshCycleDurationGOOGLE) \
	EnumMacro(PFN_vkGetPastPresentationTimingGOOGLE, vkGetPastPresentationTimingGOOGLE)

// and now, include the GenericPlatform class
#include "../VulkanGenericPlatform.h"



class FVulkanAndroidPlatform : public FVulkanGenericPlatform
{
public:
	static void SetupMaxRHIFeatureLevelAndShaderPlatform(ERHIFeatureLevel::Type InRequestedFeatureLevel);

	static bool LoadVulkanLibrary();
	static bool LoadVulkanInstanceFunctions(VkInstance inInstance);
	static void FreeVulkanLibrary();

	static void GetInstanceExtensions(TArray<const ANSICHAR*>& OutExtensions);
	static void GetDeviceExtensions(EGpuVendorId VendorId, TArray<const ANSICHAR*>& OutExtensions);

	static void CreateSurface(void* WindowHandle, VkInstance Instance, VkSurfaceKHR* OutSurface);

	static bool SupportsBCTextureFormats() { return false; }
	static bool SupportsASTCTextureFormats() { return true; }
	static bool SupportsQuerySurfaceProperties() { return false; }

	static void SetupFeatureLevels()
	{
		if (RequiresMobileRenderer())
		{
			GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES2_REMOVED] = SP_NumPlatforms;
			GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = SP_VULKAN_ES3_1_ANDROID;
			GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM4_REMOVED] = SP_NumPlatforms;
			GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM5] = SP_NumPlatforms;
		}
		else
		{
			GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES2_REMOVED] = SP_NumPlatforms;
			GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = SP_VULKAN_SM5_ANDROID;
			GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM4_REMOVED] = SP_VULKAN_SM5_ANDROID;
			GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM5] = SP_VULKAN_SM5_ANDROID;
		}
	}

	static bool SupportsStandardSwapchain();
	static EPixelFormat GetPixelFormatForNonDefaultSwapchain();

	static bool SupportsTimestampRenderQueries();

	static bool RequiresMobileRenderer()
	{
		return !FAndroidMisc::ShouldUseDesktopVulkan();
	}

	static bool SupportsVolumeTextureRendering() { return false; }

	static void OverridePlatformHandlers(bool bInit);

	//#todo-rco: Detect Mali?
	static bool RequiresPresentLayoutFix() { return true; }

	static bool HasUnifiedMemory() { return true; }

	static bool RegisterGPUWork() { return false; }

	static bool UseRealUBsOptimization(bool bCodeHeaderUseRealUBs)
	{
		return !RequiresMobileRenderer();
	}

	// Assume most devices can't use the extra cores for running parallel tasks
	static bool SupportParallelRenderingTasks() { return false; }

	//#todo-rco: Detect Mali? Doing a clear on ColorAtt layout on empty cmd buffer causes issues
	static bool RequiresSwapchainGeneralInitialLayout() { return true; }

	static bool RequiresWaitingForFrameCompletionEvent() { return false; }
	
	// Does the platform allow a nullptr Pixelshader on the pipeline
	static bool SupportsNullPixelShader() { return false; }

	static bool RequiresRenderPassResolveAttachments() { return true; }

	//#todo-rco: Detect Mali? Does the platform require depth to be written on stencil clear
	static bool RequiresDepthWriteOnStencilClear() { return true; }

protected:
	static void* VulkanLib;
	static bool bAttemptedLoad;
};

typedef FVulkanAndroidPlatform FVulkanPlatform;
