extern "C" {
#include <runtime/runtime.h>
}
#include "array.hpp"
#include "device.hpp"
#include "manager.hpp"
#include <cstdint>
#include <iostream>
#include <memory>

using namespace far_memory;
using namespace std;

// 1. Constrain local cache to a safe subset of your physical RAM (e.g., 40 GB)
constexpr uint64_t kCacheSize = (40ULL << 30); 

// 2. Set far memory size allocation large enough to handle the remainder (e.g., 45 GB)
constexpr uint64_t kFarMemSize = (45ULL << 30); 

constexpr uint32_t kNumGCThreads = 12;

struct LargeData {
    uint64_t raw_bytes[128]; // Exactly 1024 bytes per struct
};

// 3. Size must be a compile-time constexpr for the AIFM Array template template parameter
// 75 GB total target size / 1024 bytes per struct = 78,643,200 elements
constexpr uint64_t kNumElements = (75ULL * 1024 * 1024 * 1024) / sizeof(LargeData);

void do_work(FarMemManager *manager) {
    std::cout << "Allocating 75 GB using AIFM Array..." << std::endl; 
    
    // 4. Arrays must be allocated via the manager factory, not stack constructors
    auto far_array = unique_ptr<Array<LargeData, kNumElements>>(
        manager->allocate_array<LargeData, kNumElements>()
    );

    std::cout << "Writing data to Far Memory node..." << std::endl;
    for (uint64_t i = 0; i < kNumElements; i += 4096) {
        // 5. CRITICAL: DerefScope MUST be inside the loop. 
        // When this scope ends, AIFM can safely evict the element to the far node.
        DerefScope scope; 
        
        // 6. Must use .at_mut() to modify or write data to the array
        (*far_array).at_mut(scope, i).raw_bytes[0] = i;
    }

    std::cout << "Success! Application completed without running out of memory." << std::endl;
}

// 7. AIFM runs within this runtime abstraction thread instead of standard main
void _main(void *arg) {
    std::unique_ptr<FarMemManager> manager = std::unique_ptr<FarMemManager>(
        FarMemManagerFactory::build(kCacheSize, kNumGCThreads, new FakeDevice(kFarMemSize)));
    do_work(manager.get());
}

int main(int argc, char *argv[]) {
    int ret;
    if (argc < 2) {
        std::cerr << "usage: [cfg_file]" << std::endl;
        return -EINVAL;
    }

    // 8. Initializes the underlying Shenango user-space engine required by AIFM
    ret = runtime_init(argv[1], _main, NULL);
    if (ret) {
        std::cerr << "failed to start runtime" << std::endl;
        return ret;
    }

    return 0;
}
