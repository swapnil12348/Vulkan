#ifndef EXPORTED_VULKAN_FUNCTION
#define EXPORTED_VULKAN_FUNCTION( function )
#endif

EXPORTED_VULKAN_FUNCTION(vkGetInstanceProcAddr)

#undef EXPORTED_VULKAN_FUNCTION

#ifndef GLOBAL_LEVEL_VULKAN_FUNCTION
#define GLOBAL_LEVEL_VULKAN_FUNCTION( function )
#endif

// (We will add the actual global functions here later)

#undef GLOBAL_LEVEL_VULKAN_FUNCTION

#ifndef INSTANCE_LEVEL_VULKAN_FUNCTION
#define INSTANCE_LEVEL_VULKAN_FUNCTION( function )
#endif

// (We will add the actual instance functions here later)

#undef INSTANCE_LEVEL_VULKAN_FUNCTION

#ifndef INSTANCE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION
#define INSTANCE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION( function, extension )
#endif

// (We will add the actual instance extension functions here later)

#undef INSTANCE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION

#ifndef DEVICE_LEVEL_VULKAN_FUNCTION
#define DEVICE_LEVEL_VULKAN_FUNCTION( function )
#endif

// (We will add the actual device functions here later)

#undef DEVICE_LEVEL_VULKAN_FUNCTION

#ifndef DEVICE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION
#define DEVICE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION( function, extension )
#endif

// (We will add the actual device extension functions here later)

#undef DEVICE_LEVEL_VULKAN_FUNCTION_FROM_EXTENSION