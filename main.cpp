#include <windows.h>
#include <iostream>
#include <vector>            
#include "VulkanFunctions.h" 

using namespace VulkanCookbook;

bool LoadExportedFunctions(HMODULE vulkan_library) { /* ... keep exact same ... */
#if defined _WIN32
#define LoadFunction GetProcAddress
#elif defined __linux
#define LoadFunction dlsym
#endif

#define EXPORTED_VULKAN_FUNCTION( name )                                        \
        name = (PFN_##name)LoadFunction( vulkan_library, #name );                   \
        if( name == nullptr ) {                                                     \
            std::cout << "Error: Could not load exported function: " << #name << std::endl; \
            return false;                                                           \
        }

#include "ListOfVulkanFunctions.inl"
    return true;
}

bool LoadGlobalLevelFunctions() { /* ... keep exact same ... */
#define GLOBAL_LEVEL_VULKAN_FUNCTION( name )                                    \
        name = (PFN_##name)vkGetInstanceProcAddr( nullptr, #name );                 \
        if( name == nullptr ) {                                                     \
            std::cout << "Error: Could not load global function: " << #name << std::endl; \
            return false;                                                           \
        }

#include "ListOfVulkanFunctions.inl"
    return true;
}

bool CheckAvailableExtensions() { /* ... keep exact same ... */
    uint32_t extensions_count = 0;
    VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &extensions_count, nullptr);
    if (result != VK_SUCCESS || extensions_count == 0) return false;

    std::vector<VkExtensionProperties> available_extensions(extensions_count);
    result = vkEnumerateInstanceExtensionProperties(nullptr, &extensions_count, available_extensions.data());
    if (result != VK_SUCCESS) return false;

    return true;
}

// ---------------------------------------------------------
// NEW Helper 4: Create the Vulkan Instance!
// ---------------------------------------------------------
bool CreateVulkanInstance(VkInstance& out_instance) {
    // 1 & 2: Desired Extensions (We will leave this empty for now, because we don't need any to just turn Vulkan on)
    std::vector<const char*> desired_extensions = {};

    // 4: Setup the Application Info
    // Note the "{}" - this is CRITICAL in Vulkan to ensure all memory is zeroed out safely!
    VkApplicationInfo application_info = {};
    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pNext = nullptr;
    application_info.pApplicationName = "My First Vulkan Game";
    application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    application_info.pEngineName = "Custom Engine";
    application_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    application_info.apiVersion = VK_MAKE_VERSION(1, 0, 0); // We are asking for Vulkan 1.0

    // 5: Setup the Instance Create Info
    VkInstanceCreateInfo instance_create_info = {};
    instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create_info.pNext = nullptr;
    instance_create_info.flags = 0;
    instance_create_info.pApplicationInfo = &application_info;

    // We aren't turning on Validation Layers yet
    instance_create_info.enabledLayerCount = 0;
    instance_create_info.ppEnabledLayerNames = nullptr;

    // Tell Vulkan about our extensions
    instance_create_info.enabledExtensionCount = static_cast<uint32_t>(desired_extensions.size());
    instance_create_info.ppEnabledExtensionNames = desired_extensions.empty() ? nullptr : desired_extensions.data();

    // 6, 7, & 8: Finally, ask the driver to create the Instance!
    VkResult result = vkCreateInstance(&instance_create_info, nullptr, &out_instance);
    if (result != VK_SUCCESS) {
        std::cout << "Error: Could not create Vulkan Instance. Error code: " << result << std::endl;
        return false;
    }

    std::cout << "SUCCESS: THE VULKAN INSTANCE WAS CREATED!" << std::endl;
    return true;
}

// ---------------------------------------------------------
// Main Program
// ---------------------------------------------------------
int main() {
    HMODULE vulkan_library = LoadLibraryA("vulkan-1.dll");
    if (vulkan_library == nullptr) return -1;
    if (!LoadExportedFunctions(vulkan_library)) return -1;
    if (!LoadGlobalLevelFunctions()) return -1;
    if (!CheckAvailableExtensions()) return -1;

    // --- NEW CODE START ---

    // We create a blank "handle" for the instance. 
    // VK_NULL_HANDLE is Vulkan's version of nullptr.
    VkInstance instance = VK_NULL_HANDLE;

    // Pass our empty handle to the function to be filled!
    if (!CreateVulkanInstance(instance)) {
        return -1;
    }

    // --- NEW CODE END ---

    return 0;
}