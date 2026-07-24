extern "C" {
#include <base/assert.h>
#include <base/compiler.h>
#include <base/limits.h>
#include <base/stddef.h>
}

#include "object.hpp"
#include "server_ptr.hpp"

namespace far_memory {

ServerPtr::ServerPtr(uint32_t param_len, uint8_t *params) {
  uint64_t size;
  BUG_ON(param_len != sizeof(decltype(size)));
  size = *(reinterpret_cast<decltype(size) *>(params));

  // Initialize the global slab allocator for this device's memory
  base_mem_ = reinterpret_cast<uint8_t *>(malloc(size));
  slab_allocator_ = std::make_unique<Slab>(base_mem_, size);
  
  //buf_.reset(reinterpret_cast<uint8_t *>(malloc(size)));
}

ServerPtr::~ServerPtr() {
  if (base_mem_) {
        free(base_mem_);
    }
}
void ServerPtr::read_object(uint8_t obj_id_len, const uint8_t *obj_id,
                            uint16_t *data_len, uint8_t *data_buf) {
    
    const uint64_t &remote_id = *(reinterpret_cast<const uint64_t *>(obj_id));
    uint64_t true_obj_id = remote_id & kObjectIDMask;
    uint32_t partition_id = hash_virtual_id(true_obj_id) % kNumPartitions;

    uint8_t* physical_ptr = nullptr;

    // 1. Lock the specific partition and lookup
    {
        std::lock_guard<std::mutex> lock(partition_locks_[partition_id]);
        auto& table = partition_tables_[partition_id];
        
        auto it = table.find(true_obj_id);
        if (it != table.end()) {
            physical_ptr = it->second;
        }
    }

    // 2. Read outside the lock to unblock other threads
    if (physical_ptr != nullptr) {
        Object remote_object(reinterpret_cast<uint64_t>(physical_ptr));
        *data_len = remote_object.get_data_len();
        memcpy(data_buf, reinterpret_cast<uint8_t *>(remote_object.get_data_addr()), *data_len);
    } else {
        *data_len = 0; // Triggers the client's Hash Ring fallback logic
    }
}

void ServerPtr::write_object(uint8_t obj_id_len, const uint8_t *obj_id,
                             uint16_t data_len, const uint8_t *data_buf) {
                             
    const uint64_t &remote_id = *(reinterpret_cast<const uint64_t *>(obj_id));
    uint64_t true_obj_id = remote_id & kObjectIDMask;
    uint32_t partition_id = hash_virtual_id(true_obj_id) % kNumPartitions;

    uint8_t* physical_ptr = nullptr;

    // 1. Lock and lookup or allocate
    {
        std::lock_guard<std::mutex> lock(partition_locks_[partition_id]);
        auto& table = partition_tables_[partition_id];
        
        auto it = table.find(true_obj_id);
        if (it != table.end()) {
            physical_ptr = it->second;
            
            // Validate the existing allocation size
            Object old_object(reinterpret_cast<uint64_t>(physical_ptr));
            if (old_object.get_data_len() != data_len) {
                // Free the old, incorrectly sized chunk
                uint32_t free_size = old_object.get_data_len() + sizeof(Object);
                slab_allocator_->free(physical_ptr, free_size);
                
                // Allocate a fresh, correctly sized chunk
                uint32_t alloc_size = data_len + sizeof(Object); 
                physical_ptr = slab_allocator_->allocate(alloc_size);
                BUG_ON(physical_ptr == nullptr);
                
                table[true_obj_id] = physical_ptr; // Update pointer mapping
            }
        } else {
            // New Object: Request memory from the concurrent Slab allocator
            uint32_t alloc_size = data_len + sizeof(Object); 
            physical_ptr = slab_allocator_->allocate(alloc_size);
            BUG_ON(physical_ptr == nullptr); // Handle OOM
            
            table[true_obj_id] = physical_ptr;
        }
    }

    // 2. Write payload and metadata safely outside the lock
    Object remote_object(reinterpret_cast<uint64_t>(physical_ptr));
    memcpy(reinterpret_cast<uint8_t *>(remote_object.get_data_addr()), data_buf, data_len);
    remote_object.set_data_len(data_len);
    remote_object.set_obj_id_len(obj_id_len);
}

bool ServerPtr::remove_object(uint8_t obj_id_len, const uint8_t *obj_id) {
    const uint64_t &remote_id = *(reinterpret_cast<const uint64_t *>(obj_id));
    uint64_t true_obj_id = remote_id & kObjectIDMask;
    uint32_t partition_id = hash_virtual_id(true_obj_id) % kNumPartitions;

    uint8_t* physical_ptr = nullptr;

    {
        std::lock_guard<std::mutex> lock(partition_locks_[partition_id]);
        auto& table = partition_tables_[partition_id];
        auto it = table.find(true_obj_id);
        
        if (it != table.end()) {
            physical_ptr = it->second;
            table.erase(it);
        } else {
            return false;
        }
    }

    // Free back to the Slab allocator using the correct size
    Object remote_object(reinterpret_cast<uint64_t>(physical_ptr));
    uint32_t free_size = remote_object.get_data_len() + sizeof(Object);
    slab_allocator_->free(physical_ptr, free_size);
    return true;
}

void ServerPtr::compute(uint8_t opcode, uint16_t input_len,
                        const uint8_t *input_buf, uint16_t *output_len,
                        uint8_t *output_buf) {
  BUG();
}

ServerDS *ServerPtrFactory::build(uint32_t param_len, uint8_t *params) {
  return new ServerPtr(param_len, params);
}

}; // namespace far_memory
