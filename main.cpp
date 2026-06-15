#include <windows.h>
#include <iostream>
#include <vulkan/vulkan.h> // Tells this file what "PFN_vkGetInstanceProcAddr" is

int main() {
    // 1. Load the Vulkan library
    HMODULE vulkan_library = LoadLibraryA("vulkan-1.dll");

    if (vulkan_library != nullptr) {
        std::cout << "Success: vulkan-1.dll was loaded perfectly!" << std::endl;
    }
    else {
        std::cout << "Error: Could not load vulkan-1.dll." << std::endl;
        return -1;
    }

    // --- NEW CODE START ---

    // 2. Extract the "Master Key" function from the DLL using Windows' GetProcAddress
    // "PFN_" stands for "Pointer to FunctioN"
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr =
        (PFN_vkGetInstanceProcAddr)GetProcAddress(vulkan_library, "vkGetInstanceProcAddr");

    // 3. Confirm that we successfully found the Master Key
    if (vkGetInstanceProcAddr != nullptr) {
        std::cout << "Success: Loaded the master function (vkGetInstanceProcAddr)!" << std::endl;
    }
    else {
        std::cout << "Error: Could not load the master function." << std::endl;
        return -1;
    }

    // --- NEW CODE END ---

    return 0;
}