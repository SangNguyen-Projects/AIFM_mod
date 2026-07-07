// native_crash.cpp
#include <iostream>
#include <vector>

struct LargeData {
    uint64_t raw_bytes[128]; // 1024 bytes per struct
};

int main() {
    std::cout << "Attempting to allocate 75 GB of local DRAM..." << std::endl;
    
    // 75 GB * 1024 MB * 1024 KB * 1024 Bytes / 1024 Bytes per struct
    uint64_t num_elements = 75ULL * 1024 * 1024 * 1024 / sizeof(LargeData);
    
    try {
        // Will throw std::bad_alloc or trigger OS OOM killer
        std::vector<LargeData> local_array(num_elements);
        
        // Force the OS to touch pages (write data)
        for (uint64_t i = 0; i < num_elements; i += 4096) {
            local_array[i].raw_bytes[0] = i;
        }
        std::cout << "Success!" << std::endl; // Will never reach this line
    } catch (const std::bad_alloc& e) {
        std::cerr << "Allocation failed: Not enough DRAM!" << std::endl;
    }
    return 0;
}
