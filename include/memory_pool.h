/**
 * @file memory_pool.h
 * @brief This file includes the declaration of the MemoryPool class.
 * A memory pool is a memory allocation that creates a pool of memory
 * that is uniform across blocks that can be used for heap memory in
 * a system without using global heap except for once at initialization.
 * 
 */
#pragma once

#include <cstddef>
#include <mutex>
#include <cassert>
#include <memory>

/**
 * @brief The MemoryPool class is a memory pool allocator that allocates
 * memory blocks of a fixed size. 
 * 
 * The memory pool is created with a fixed number of blocks that are all 
 * the same size. The memory pool is thread-safe.
 */
class MemoryPool
{
public:
  /**
   * @brief Construct a new Memory Pool object
   * 
   * @param block_size The size of each memory block in bytes.
   * @param block_count The number of memory blocks to allocate.
   */
  MemoryPool(std::size_t block_size, std::size_t block_count);

  /**
   * @brief Destroy the Memory Pool object
   */
  ~MemoryPool();

  /**
   * @brief Allocate a memory block from the memory pool.
   * 
   * @return void* A pointer to the allocated memory block.
   */
  void* allocate();

  /**
   * @brief Deallocate a memory block and return it to the memory pool.
   * 
   * @param ptr A pointer to the memory block to deallocate.
   */
  void deallocate(void* ptr);

private:
  // Disable copy and assignment for the memory pool
  MemoryPool(const MemoryPool&) = delete;
  MemoryPool& operator=(const MemoryPool&) = delete;

  std::size_t block_size;                 ///< The size of each memory block in bytes.
  std::size_t block_count;                ///< The number of memory blocks in the pool.
  std::unique_ptr<std::byte[]> pool_data; ///< The memory pool data.
  std::byte* free_list;                   ///< The list of free memory blocks.

  mutable std::mutex pool_mutex; ///< The mutex to protect the memory pool.
};
