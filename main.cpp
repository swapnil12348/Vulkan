#include <windows.h>
#include <iostream>
#include "VulkanFunctions.h" // We must include this so our program knows the variables exist!

// Bring our generated variables into scope
using namespace VulkanCookbook;

// ---------------------------------------------------------
// Helper Function: Loads all exported functions automatically
// ---------------------------------------------------------
bool LoadExportedFunctions(HMODULE vulkan_library) {
    // Determine the correct OS command (GetProcAddress for Windows)
#if defined _WIN32
#define LoadFunction GetProcAddress
#elif defined __linux
#define LoadFunction dlsym
#endif

// This macro automatically writes the GetProcAddress code for ANY function name we give it
#define EXPORTED_VULKAN_FUNCTION( name )                                        \
        name = (PFN_##name)LoadFunction( vulkan_library, #name );                   \
        if( name == nullptr ) {                                                     \
            std::cout << "Error: Could not load exported function: " << #name << std::endl; \
            return false;                                                           \
        }

    // By including the list here, the macro above runs on every single item in the list!
#include "ListOfVulkanFunctions.inl"

    return true; // If it makes it through the whole list without failing, return true
}

// ---------------------------------------------------------
// Main Program
// ---------------------------------------------------------
int main() {
    // 1. Load the Vulkan driver
    HMODULE vulkan_library = LoadLibraryA("vulkan-1.dll");
    if (vulkan_library == nullptr) {
        std::cout << "Error: Could not load vulkan-1.dll." << std::endl;
        return -1;
    }
    std::cout << "Success: vulkan-1.dll loaded!" << std::endl;

    // 2. Use our new macro function to load the Master Key automatically
    if (!LoadExportedFunctions(vulkan_library)) {
        return -1; // If it failed to load, stop the program
    }
    std::cout << "Success: Exported functions loaded automatically!" << std::endl;

    return 0;
}