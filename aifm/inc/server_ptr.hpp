#pragma once

#include "server_ds.hpp"
#include "slab.hpp" // Required for the Slab allocator

#include <memory>
#include <unordered_map> // Required for partition_tables_
#include <mutex>         // Required for partition_locks_

namespace far_memory {
class ServerPtr : public ServerDS {
private:
  //std::unique_ptr<uint8_t> buf_;
  friend class ServerPtrFactory;
  uint8_t* base_mem_;
  
  static constexpr uint32_t kNumPartitions = 4096;
  static constexpr uint64_t kObjectIDMask = (1ULL << 38) - 1;

  inline uint64_t hash_virtual_id(uint64_t x) const {
      x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
      x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
      return x ^ (x >> 31);
  }

  // The global physical memory allocator
  std::unique_ptr<Slab> slab_allocator_;

  // Sharded Page Tables for vBuckets
  std::unordered_map<uint64_t, uint8_t*> partition_tables_[kNumPartitions];
  std::mutex partition_locks_[kNumPartitions];

public:
  ServerPtr(uint32_t param_len, uint8_t *params);
  ~ServerPtr();
  void read_object(uint8_t obj_id_len, const uint8_t *obj_id,
                   uint16_t *data_len, uint8_t *data_buf);
  void write_object(uint8_t obj_id_len, const uint8_t *obj_id,
                    uint16_t data_len, const uint8_t *data_buf);
  bool remove_object(uint8_t obj_id_len, const uint8_t *obj_id);
  void compute(uint8_t opcode, uint16_t input_len, const uint8_t *input_buf,
               uint16_t *output_len, uint8_t *output_buf);
};

class ServerPtrFactory : public ServerDSFactory {
public:
  ServerDS *build(uint32_t param_len, uint8_t *params);
};
} // namespace far_memory
