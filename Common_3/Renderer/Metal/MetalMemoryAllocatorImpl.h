#pragma once

// Ignore warnings from VMA
#pragma clang diagnostic push
#pragma clang diagnostic push
#pragma clang diagnostic push
#pragma clang diagnostic push
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-private-field"
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wnullability-completeness"
#define VMA_IMPLEMENTATION
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0

#define VULKAN_H_

#define VK_MAKE_VERSION(major, minor, patch) \
    (((major) << 22) | ((minor) << 12) | (patch))

// DEPRECATED: This define has been removed. Specific version defines (e.g. VK_API_VERSION_1_0), or the VK_MAKE_VERSION macro, should be used instead.
//#define VK_API_VERSION VK_MAKE_VERSION(1, 0, 0)

// Vulkan 1.0 version number
#define VK_API_VERSION_1_0 VK_MAKE_VERSION(1, 0, 0)

#define VK_VERSION_MAJOR(version) ((uint32_t)(version) >> 22)
#define VK_VERSION_MINOR(version) (((uint32_t)(version) >> 12) & 0x3ff)
#define VK_VERSION_PATCH(version) ((uint32_t)(version) & 0xfff)

#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
// On other platforms, use the default calling convention
#define VK_NO_PROTOTYPES
#define VKAPI_ATTR
#define VKAPI_CALL
#define VKAPI_PTR
#define VK_SYSTEM_ALLOCATION_SCOPE_OBJECT 0
#define VK_NULL_HANDLE NULL
#define VK_WHOLE_SIZE UINT64_MAX
#define VK_FALSE 0
#define VK_TRUE 1

#define VK_MAX_MEMORY_TYPES               RESOURCE_MEMORY_USAGE_COUNT
#ifdef TARGET_IOS
#define VK_MAX_MEMORY_HEAPS               1
#else
// Even on integrated macs there is some memory for gpus usually around 1.5 GB
// This makes it better to separate the heaps as device local and shared
#define VK_MAX_MEMORY_HEAPS               2
#endif

VK_DEFINE_HANDLE(VkDeviceMemory)

struct VkDeviceMemory_T
{
	id pHeap;
};

typedef void* VkInstance;
typedef Renderer* VkPhysicalDevice;
typedef void* VkCommandBuffer;
typedef uint64_t VkDeviceSize;
typedef uint32_t VkFlags;
typedef uint8_t VkBool32;
typedef uint32_t VkSystemAllocationScope;
typedef uint32_t VkInternalAllocationType;
typedef void* VkBuffer;
typedef void* VkImage;
typedef VkFlags VkMemoryMapFlags;
typedef VkFlags VkBufferCreateFlags;

typedef Renderer* VkDevice;

typedef enum VkResult
{
	VK_SUCCESS = 0,
	VK_NOT_READY = 1,
	VK_TIMEOUT = 2,
	VK_EVENT_SET = 3,
	VK_EVENT_RESET = 4,
	VK_INCOMPLETE = 5,
	VK_ERROR_OUT_OF_HOST_MEMORY = -1,
	VK_ERROR_OUT_OF_DEVICE_MEMORY = -2,
	VK_ERROR_INITIALIZATION_FAILED = -3,
	VK_ERROR_DEVICE_LOST = -4,
	VK_ERROR_MEMORY_MAP_FAILED = -5,
	VK_ERROR_LAYER_NOT_PRESENT = -6,
	VK_ERROR_EXTENSION_NOT_PRESENT = -7,
	VK_ERROR_FEATURE_NOT_PRESENT = -8,
	VK_ERROR_INCOMPATIBLE_DRIVER = -9,
	VK_ERROR_TOO_MANY_OBJECTS = -10,
	VK_ERROR_FORMAT_NOT_SUPPORTED = -11,
	VK_ERROR_FRAGMENTED_POOL = -12,
	VK_ERROR_OUT_OF_POOL_MEMORY = -1000069000,
	VK_ERROR_INVALID_EXTERNAL_HANDLE = -1000072003,
	VK_ERROR_SURFACE_LOST_KHR = -1000000000,
	VK_ERROR_NATIVE_WINDOW_IN_USE_KHR = -1000000001,
	VK_SUBOPTIMAL_KHR = 1000001003,
	VK_ERROR_OUT_OF_DATE_KHR = -1000001004,
	VK_ERROR_INCOMPATIBLE_DISPLAY_KHR = -1000003001,
	VK_ERROR_VALIDATION_FAILED_EXT = -1000011001,
	VK_ERROR_INVALID_SHADER_NV = -1000012000,
	VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT = -1000158000,
	VK_ERROR_FRAGMENTATION_EXT = -1000161000,
	VK_ERROR_NOT_PERMITTED_EXT = -1000174001,
	VK_ERROR_INVALID_DEVICE_ADDRESS_EXT = -1000244000,
	VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT = -1000255000,
	VK_ERROR_OUT_OF_POOL_MEMORY_KHR = VK_ERROR_OUT_OF_POOL_MEMORY,
	VK_ERROR_INVALID_EXTERNAL_HANDLE_KHR = VK_ERROR_INVALID_EXTERNAL_HANDLE,
	VK_RESULT_BEGIN_RANGE = VK_ERROR_FRAGMENTED_POOL,
	VK_RESULT_END_RANGE = VK_INCOMPLETE,
	VK_RESULT_RANGE_SIZE = (VK_INCOMPLETE - VK_ERROR_FRAGMENTED_POOL + 1),
	VK_RESULT_MAX_ENUM = 0x7FFFFFFF
} VkResult;

typedef enum VkStructureType {
	VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO = 0,
	VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
	VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
	VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
} VkStructureType;

typedef enum VkMemoryPropertyFlagBits {
	VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT = 0x00000001,
	VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT = 0x00000002,
	VK_MEMORY_PROPERTY_HOST_COHERENT_BIT = 0x00000004,
	VK_MEMORY_PROPERTY_HOST_CACHED_BIT = 0x00000008,
	VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT = 0x00000010,
	VK_MEMORY_PROPERTY_PROTECTED_BIT = 0x00000020,
	VK_MEMORY_PROPERTY_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
} VkMemoryPropertyFlagBits;
typedef VkFlags VkMemoryPropertyFlags;

typedef enum VkMemoryHeapFlagBits {
	VK_MEMORY_HEAP_DEVICE_LOCAL_BIT = 0x00000001,
	VK_MEMORY_HEAP_MULTI_INSTANCE_BIT = 0x00000002,
	VK_MEMORY_HEAP_MULTI_INSTANCE_BIT_KHR = VK_MEMORY_HEAP_MULTI_INSTANCE_BIT,
	VK_MEMORY_HEAP_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
} VkMemoryHeapFlagBits;
typedef VkFlags VkMemoryHeapFlags;

typedef void* (VKAPI_PTR *PFN_vkAllocationFunction)(
	void*                                       pUserData,
	size_t                                      size,
	size_t                                      alignment,
	VkSystemAllocationScope                     allocationScope);

typedef void* (VKAPI_PTR *PFN_vkReallocationFunction)(
	void*                                       pUserData,
	void*                                       pOriginal,
	size_t                                      size,
	size_t                                      alignment,
	VkSystemAllocationScope                     allocationScope);

typedef void (VKAPI_PTR *PFN_vkFreeFunction)(
	void*                                       pUserData,
	void*                                       pMemory);

typedef void (VKAPI_PTR *PFN_vkInternalAllocationNotification)(
	void*                                       pUserData,
	size_t                                      size,
	VkInternalAllocationType                    allocationType,
	VkSystemAllocationScope                     allocationScope);

typedef void (VKAPI_PTR *PFN_vkInternalFreeNotification)(
	void*                                       pUserData,
	size_t                                      size,
	VkInternalAllocationType                    allocationType,
	VkSystemAllocationScope                     allocationScope);

typedef struct VkAllocationCallbacks {
	void*                                   pUserData;
	PFN_vkAllocationFunction                pfnAllocation;
	PFN_vkReallocationFunction              pfnReallocation;
	PFN_vkFreeFunction                      pfnFree;
	PFN_vkInternalAllocationNotification    pfnInternalAllocation;
	PFN_vkInternalFreeNotification          pfnInternalFree;
} VkAllocationCallbacks;

typedef enum VkPhysicalDeviceType {
	VK_PHYSICAL_DEVICE_TYPE_OTHER = 0,
	VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU = 1,
	VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU = 2,
	VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU = 3,
	VK_PHYSICAL_DEVICE_TYPE_CPU = 4,
	VK_PHYSICAL_DEVICE_TYPE_BEGIN_RANGE = VK_PHYSICAL_DEVICE_TYPE_OTHER,
	VK_PHYSICAL_DEVICE_TYPE_END_RANGE = VK_PHYSICAL_DEVICE_TYPE_CPU,
	VK_PHYSICAL_DEVICE_TYPE_RANGE_SIZE = (VK_PHYSICAL_DEVICE_TYPE_CPU - VK_PHYSICAL_DEVICE_TYPE_OTHER + 1),
	VK_PHYSICAL_DEVICE_TYPE_MAX_ENUM = 0x7FFFFFFF
} VkPhysicalDeviceType;

typedef struct VkPhysicalDeviceLimits {
	uint32_t              maxImageDimension1D;
	uint32_t              maxImageDimension2D;
	uint32_t              maxImageDimension3D;
	uint32_t              maxImageDimensionCube;
	uint32_t              maxImageArrayLayers;
	uint32_t              maxTexelBufferElements;
	uint32_t              maxUniformBufferRange;
	uint32_t              maxStorageBufferRange;
	uint32_t              maxPushConstantsSize;
	uint32_t              maxMemoryAllocationCount;
	uint32_t              maxSamplerAllocationCount;
	VkDeviceSize          bufferImageGranularity;
	VkDeviceSize          sparseAddressSpaceSize;
	uint32_t              maxBoundDescriptorSets;
	uint32_t              maxPerStageDescriptorSamplers;
	uint32_t              maxPerStageDescriptorUniformBuffers;
	uint32_t              maxPerStageDescriptorStorageBuffers;
	uint32_t              maxPerStageDescriptorSampledImages;
	uint32_t              maxPerStageDescriptorStorageImages;
	uint32_t              maxPerStageDescriptorInputAttachments;
	uint32_t              maxPerStageResources;
	uint32_t              maxDescriptorSetSamplers;
	uint32_t              maxDescriptorSetUniformBuffers;
	uint32_t              maxDescriptorSetUniformBuffersDynamic;
	uint32_t              maxDescriptorSetStorageBuffers;
	uint32_t              maxDescriptorSetStorageBuffersDynamic;
	uint32_t              maxDescriptorSetSampledImages;
	uint32_t              maxDescriptorSetStorageImages;
	uint32_t              maxDescriptorSetInputAttachments;
	uint32_t              maxVertexInputAttributes;
	uint32_t              maxVertexInputBindings;
	uint32_t              maxVertexInputAttributeOffset;
	uint32_t              maxVertexInputBindingStride;
	uint32_t              maxVertexOutputComponents;
	uint32_t              maxTessellationGenerationLevel;
	uint32_t              maxTessellationPatchSize;
	uint32_t              maxTessellationControlPerVertexInputComponents;
	uint32_t              maxTessellationControlPerVertexOutputComponents;
	uint32_t              maxTessellationControlPerPatchOutputComponents;
	uint32_t              maxTessellationControlTotalOutputComponents;
	uint32_t              maxTessellationEvaluationInputComponents;
	uint32_t              maxTessellationEvaluationOutputComponents;
	uint32_t              maxGeometryShaderInvocations;
	uint32_t              maxGeometryInputComponents;
	uint32_t              maxGeometryOutputComponents;
	uint32_t              maxGeometryOutputVertices;
	uint32_t              maxGeometryTotalOutputComponents;
	uint32_t              maxFragmentInputComponents;
	uint32_t              maxFragmentOutputAttachments;
	uint32_t              maxFragmentDualSrcAttachments;
	uint32_t              maxFragmentCombinedOutputResources;
	uint32_t              maxComputeSharedMemorySize;
	uint32_t              maxComputeWorkGroupCount[3];
	uint32_t              maxComputeWorkGroupInvocations;
	uint32_t              maxComputeWorkGroupSize[3];
	uint32_t              subPixelPrecisionBits;
	uint32_t              subTexelPrecisionBits;
	uint32_t              mipmapPrecisionBits;
	uint32_t              maxDrawIndexedIndexValue;
	uint32_t              maxDrawIndirectCount;
	float                 maxSamplerLodBias;
	float                 maxSamplerAnisotropy;
	uint32_t              maxViewports;
	uint32_t              maxViewportDimensions[2];
	float                 viewportBoundsRange[2];
	uint32_t              viewportSubPixelBits;
	size_t                minMemoryMapAlignment;
	VkDeviceSize          minTexelBufferOffsetAlignment;
	VkDeviceSize          minUniformBufferOffsetAlignment;
	VkDeviceSize          minStorageBufferOffsetAlignment;
	int32_t               minTexelOffset;
	uint32_t              maxTexelOffset;
	int32_t               minTexelGatherOffset;
	uint32_t              maxTexelGatherOffset;
	float                 minInterpolationOffset;
	float                 maxInterpolationOffset;
	uint32_t              subPixelInterpolationOffsetBits;
	uint32_t              maxFramebufferWidth;
	uint32_t              maxFramebufferHeight;
	uint32_t              maxFramebufferLayers;
	uint32_t              maxColorAttachments;
	uint32_t              maxSampleMaskWords;
	VkBool32              timestampComputeAndGraphics;
	float                 timestampPeriod;
	uint32_t              maxClipDistances;
	uint32_t              maxCullDistances;
	uint32_t              maxCombinedClipAndCullDistances;
	uint32_t              discreteQueuePriorities;
	float                 pointSizeRange[2];
	float                 lineWidthRange[2];
	float                 pointSizeGranularity;
	float                 lineWidthGranularity;
	VkBool32              strictLines;
	VkBool32              standardSampleLocations;
	VkDeviceSize          optimalBufferCopyOffsetAlignment;
	VkDeviceSize          optimalBufferCopyRowPitchAlignment;
	VkDeviceSize          nonCoherentAtomSize;
} VkPhysicalDeviceLimits;

typedef struct VkPhysicalDeviceProperties {
	uint32_t                            apiVersion;
	uint32_t                            driverVersion;
	uint32_t                            vendorID;
	uint32_t                            deviceID;
	VkPhysicalDeviceType                deviceType;
	char                                deviceName[256];
	uint8_t                             pipelineCacheUUID[256];
	VkPhysicalDeviceLimits              limits;
} VkPhysicalDeviceProperties;

typedef struct VkMemoryType {
	VkMemoryPropertyFlags    propertyFlags;
	uint32_t                 heapIndex;
} VkMemoryType;

typedef struct VkMemoryHeap {
	VkDeviceSize         size;
	VkMemoryHeapFlags    flags;
} VkMemoryHeap;

typedef struct VkPhysicalDeviceMemoryProperties {
	uint32_t        memoryTypeCount;
	VkMemoryType    memoryTypes[VK_MAX_MEMORY_TYPES];
	uint32_t        memoryHeapCount;
	VkMemoryHeap    memoryHeaps[VK_MAX_MEMORY_HEAPS];
} VkPhysicalDeviceMemoryProperties;

typedef struct VkMemoryRequirements {
	VkDeviceSize    size;
	VkDeviceSize    alignment;
	uint32_t        memoryTypeBits;
} VkMemoryRequirements;

typedef struct VkMemoryAllocateInfo {
	VkStructureType    sType;
	const void*        pNext;
	VkDeviceSize       allocationSize;
	uint32_t           memoryTypeIndex;
} VkMemoryAllocateInfo;

typedef struct VkMappedMemoryRange {
	VkStructureType    sType;
	const void*        pNext;
	VkDeviceMemory     memory;
	VkDeviceSize       offset;
	VkDeviceSize       size;
} VkMappedMemoryRange;

typedef struct VkBufferCopy {
	VkDeviceSize    srcOffset;
	VkDeviceSize    dstOffset;
	VkDeviceSize    size;
} VkBufferCopy;

typedef enum VkSharingMode {
	VK_SHARING_MODE_EXCLUSIVE = 0,
	VK_SHARING_MODE_CONCURRENT = 1,
	VK_SHARING_MODE_BEGIN_RANGE = VK_SHARING_MODE_EXCLUSIVE,
	VK_SHARING_MODE_END_RANGE = VK_SHARING_MODE_CONCURRENT,
	VK_SHARING_MODE_RANGE_SIZE = (VK_SHARING_MODE_CONCURRENT - VK_SHARING_MODE_EXCLUSIVE + 1),
	VK_SHARING_MODE_MAX_ENUM = 0x7FFFFFFF
} VkSharingMode;

typedef enum VkBufferCreateFlagBits {
	VK_BUFFER_CREATE_SPARSE_BINDING_BIT = 0x00000001,
	VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT = 0x00000002,
	VK_BUFFER_CREATE_SPARSE_ALIASED_BIT = 0x00000004,
	VK_BUFFER_CREATE_PROTECTED_BIT = 0x00000008,
	VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT_EXT = 0x00000010,
	VK_BUFFER_CREATE_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
} VkBufferCreateFlagBits;
typedef VkFlags VkBufferCreateFlags;

typedef enum VkBufferUsageFlagBits {
	VK_BUFFER_USAGE_TRANSFER_SRC_BIT = 0x00000001,
	VK_BUFFER_USAGE_TRANSFER_DST_BIT = 0x00000002,
	VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT = 0x00000004,
	VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT = 0x00000008,
	VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT = 0x00000010,
	VK_BUFFER_USAGE_STORAGE_BUFFER_BIT = 0x00000020,
	VK_BUFFER_USAGE_INDEX_BUFFER_BIT = 0x00000040,
	VK_BUFFER_USAGE_VERTEX_BUFFER_BIT = 0x00000080,
	VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT = 0x00000100,
	VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT = 0x00000800,
	VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT = 0x00001000,
	VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT = 0x00000200,
	VK_BUFFER_USAGE_RAY_TRACING_BIT_NV = 0x00000400,
	VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT = 0x00020000,
	VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
} VkBufferUsageFlagBits;
typedef VkFlags VkBufferUsageFlags;

typedef struct VkBufferCreateInfo {
	VkStructureType        sType;
	const void*            pNext;
	VkBufferCreateFlags    flags;
	VkDeviceSize           size;
	VkBufferUsageFlags     usage;
	VkSharingMode          sharingMode;
	uint32_t               queueFamilyIndexCount;
	const uint32_t*        pQueueFamilyIndices;
} VkBufferCreateInfo;

typedef enum VkImageUsageFlagBits {
	VK_IMAGE_USAGE_TRANSFER_SRC_BIT = 0x00000001,
	VK_IMAGE_USAGE_TRANSFER_DST_BIT = 0x00000002,
	VK_IMAGE_USAGE_SAMPLED_BIT = 0x00000004,
	VK_IMAGE_USAGE_STORAGE_BIT = 0x00000008,
	VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT = 0x00000010,
	VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT = 0x00000020,
	VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT = 0x00000040,
	VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT = 0x00000080,
	VK_IMAGE_USAGE_SHADING_RATE_IMAGE_BIT_NV = 0x00000100,
	VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT = 0x00000200,
	VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
} VkImageUsageFlagBits;
typedef VkFlags VkImageUsageFlags;

typedef enum VkImageCreateFlagBits {
	VK_IMAGE_CREATE_SPARSE_BINDING_BIT = 0x00000001,
	VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT = 0x00000002,
	VK_IMAGE_CREATE_SPARSE_ALIASED_BIT = 0x00000004,
	VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT = 0x00000008,
	VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT = 0x00000010,
	VK_IMAGE_CREATE_ALIAS_BIT = 0x00000400,
	VK_IMAGE_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT = 0x00000040,
	VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT = 0x00000020,
	VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT = 0x00000080,
	VK_IMAGE_CREATE_EXTENDED_USAGE_BIT = 0x00000100,
	VK_IMAGE_CREATE_PROTECTED_BIT = 0x00000800,
	VK_IMAGE_CREATE_DISJOINT_BIT = 0x00000200,
	VK_IMAGE_CREATE_CORNER_SAMPLED_BIT_NV = 0x00002000,
	VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT = 0x00001000,
	VK_IMAGE_CREATE_SUBSAMPLED_BIT_EXT = 0x00004000,
	VK_IMAGE_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT_KHR = VK_IMAGE_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT,
	VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT_KHR = VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT,
	VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT_KHR = VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT,
	VK_IMAGE_CREATE_EXTENDED_USAGE_BIT_KHR = VK_IMAGE_CREATE_EXTENDED_USAGE_BIT,
	VK_IMAGE_CREATE_DISJOINT_BIT_KHR = VK_IMAGE_CREATE_DISJOINT_BIT,
	VK_IMAGE_CREATE_ALIAS_BIT_KHR = VK_IMAGE_CREATE_ALIAS_BIT,
	VK_IMAGE_CREATE_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
} VkImageCreateFlagBits;
typedef VkFlags VkImageCreateFlags;

typedef enum VkImageType {
	VK_IMAGE_TYPE_1D = 0,
	VK_IMAGE_TYPE_2D = 1,
	VK_IMAGE_TYPE_3D = 2,
	VK_IMAGE_TYPE_BEGIN_RANGE = VK_IMAGE_TYPE_1D,
	VK_IMAGE_TYPE_END_RANGE = VK_IMAGE_TYPE_3D,
	VK_IMAGE_TYPE_RANGE_SIZE = (VK_IMAGE_TYPE_3D - VK_IMAGE_TYPE_1D + 1),
	VK_IMAGE_TYPE_MAX_ENUM = 0x7FFFFFFF
} VkImageType;

typedef uint32_t VkFormat;

typedef enum VkImageTiling {
	VK_IMAGE_TILING_OPTIMAL = 0,
	VK_IMAGE_TILING_LINEAR = 1,
	VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT = 1000158000,
	VK_IMAGE_TILING_BEGIN_RANGE = VK_IMAGE_TILING_OPTIMAL,
	VK_IMAGE_TILING_END_RANGE = VK_IMAGE_TILING_LINEAR,
	VK_IMAGE_TILING_RANGE_SIZE = (VK_IMAGE_TILING_LINEAR - VK_IMAGE_TILING_OPTIMAL + 1),
	VK_IMAGE_TILING_MAX_ENUM = 0x7FFFFFFF
} VkImageTiling;

typedef struct VkExtent3D {
	uint32_t    width;
	uint32_t    height;
	uint32_t    depth;
} VkExtent3D;

typedef enum VkSampleCountFlagBits {
	VK_SAMPLE_COUNT_1_BIT = 0x00000001,
	VK_SAMPLE_COUNT_2_BIT = 0x00000002,
	VK_SAMPLE_COUNT_4_BIT = 0x00000004,
	VK_SAMPLE_COUNT_8_BIT = 0x00000008,
	VK_SAMPLE_COUNT_16_BIT = 0x00000010,
	VK_SAMPLE_COUNT_32_BIT = 0x00000020,
	VK_SAMPLE_COUNT_64_BIT = 0x00000040,
	VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
} VkSampleCountFlagBits;
typedef VkFlags VkSampleCountFlags;

typedef enum VkImageLayout {
	VK_IMAGE_LAYOUT_UNDEFINED = 0,
	VK_IMAGE_LAYOUT_GENERAL = 1,
	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL = 2,
	VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL = 3,
	VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL = 4,
	VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL = 5,
	VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL = 6,
	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL = 7,
	VK_IMAGE_LAYOUT_PREINITIALIZED = 8,
	VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL = 1000117000,
	VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL = 1000117001,
	VK_IMAGE_LAYOUT_PRESENT_SRC_KHR = 1000001002,
	VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR = 1000111000,
	VK_IMAGE_LAYOUT_SHADING_RATE_OPTIMAL_NV = 1000164003,
	VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT = 1000218000,
	VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL,
	VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL,
	VK_IMAGE_LAYOUT_BEGIN_RANGE = VK_IMAGE_LAYOUT_UNDEFINED,
	VK_IMAGE_LAYOUT_END_RANGE = VK_IMAGE_LAYOUT_PREINITIALIZED,
	VK_IMAGE_LAYOUT_RANGE_SIZE = (VK_IMAGE_LAYOUT_PREINITIALIZED - VK_IMAGE_LAYOUT_UNDEFINED + 1),
	VK_IMAGE_LAYOUT_MAX_ENUM = 0x7FFFFFFF
} VkImageLayout;

typedef struct VkImageCreateInfo {
	VkStructureType          sType;
	const void*              pNext;
	VkImageCreateFlags       flags;
	VkImageType              imageType;
	VkFormat                 format;
	VkExtent3D               extent;
	uint32_t                 mipLevels;
	uint32_t                 arrayLayers;
	VkSampleCountFlagBits    samples;
	VkImageTiling            tiling;
	VkImageUsageFlags        usage;
	VkSharingMode            sharingMode;
	uint32_t                 queueFamilyIndexCount;
	const uint32_t*          pQueueFamilyIndices;
	VkImageLayout            initialLayout;
} VkImageCreateInfo;

typedef void (VKAPI_PTR *PFN_vkGetPhysicalDeviceProperties)(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties);
typedef void (VKAPI_PTR *PFN_vkGetPhysicalDeviceMemoryProperties)(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties);
typedef VkResult (VKAPI_PTR *PFN_vkAllocateMemory)(VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory);
typedef void (VKAPI_PTR *PFN_vkFreeMemory)(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* pAllocator);
typedef VkResult (VKAPI_PTR *PFN_vkMapMemory)(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData);
typedef void (VKAPI_PTR *PFN_vkUnmapMemory)(VkDevice device, VkDeviceMemory memory);
typedef VkResult (VKAPI_PTR *PFN_vkFlushMappedMemoryRanges)(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges);
typedef VkResult (VKAPI_PTR *PFN_vkInvalidateMappedMemoryRanges)(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges);
typedef VkResult (VKAPI_PTR *PFN_vkBindBufferMemory)(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset);
typedef VkResult (VKAPI_PTR *PFN_vkBindImageMemory)(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset);
typedef void (VKAPI_PTR *PFN_vkGetBufferMemoryRequirements)(VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements);
typedef void (VKAPI_PTR *PFN_vkGetImageMemoryRequirements)(VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements);
typedef VkResult (VKAPI_PTR *PFN_vkCreateBuffer)(VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer);
typedef void (VKAPI_PTR *PFN_vkDestroyBuffer)(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator);
typedef VkResult (VKAPI_PTR *PFN_vkCreateImage)(VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage);
typedef void (VKAPI_PTR *PFN_vkDestroyImage)(VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator);
typedef void (VKAPI_PTR *PFN_vkCmdCopyBuffer)(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions);

#include "MetalMemoryAllocator.h"
#pragma clang diagnostic pop
#pragma clang diagnostic pop
#pragma clang diagnostic pop
#pragma clang diagnostic pop
#pragma clang diagnostic pop
