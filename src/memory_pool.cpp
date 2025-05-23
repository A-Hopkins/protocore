/**
 * @file memory_pool.cpp
 * @brief This file includes the implementation of the MemoryPool class.
 *
 * A memory pool is a memory allocation that creates a pool of memory
 * that is uniform across blocks that can be used for heap memory in
 * a system without using global heap except for once at initialization.
 */
#include "memory_pool.h"

MemoryPool::MemoryPool(std::size_t block_size, std::size_t block_count) : free_list(nullptr)
{
  // Validate the parameters
  // block_size must be greater than the size of a pointer
  // block_count must be greater than 0
  assert(block_count > 0 && "block_count must be greater than 0");
  assert(block_size > sizeof(void*) && "block_size must be greater than a pointer size");

  this->block_size  = block_size;
  this->block_count = block_count;

  // Allocate the memory pool
  // The memory pool is a contiguous block of memory
  // The size of the memory pool is block_size * block_count
  pool_data = std::make_unique<std::byte[]>(block_size * block_count);
  free_list = pool_data.get();

  // Initialize the free list
  // The free list is a linked list of memory blocks
  // Each block points to the next block in the free list
  // The last block points to nullptr
  std::byte* current_block = free_list;
  for (std::size_t i = 0; i < block_count - 1; ++i)
  {
    std::byte* next                               = current_block + block_size;
    *reinterpret_cast<std::byte**>(current_block) = next;
    current_block                                 = next;
  }

  *reinterpret_cast<std::byte**>(current_block) = nullptr;
}

MemoryPool::~MemoryPool() = default;

void* MemoryPool::allocate()
{
  std::lock_guard<std::mutex> lock(pool_mutex);

  if (!free_list)
  {
    return nullptr;
  }

  // Validate that the free_list pointer is within the pool area
  assert(free_list >= pool_data.get() && free_list < pool_data.get() + block_size * block_count &&
         "free_list pointer is out of range");

  std::byte* allocated_block = free_list;
  // Update free_list to the next available block.
  free_list = *reinterpret_cast<std::byte**>(free_list);
  return allocated_block;
}

void MemoryPool::deallocate(void* ptr)
{
  std::lock_guard<std::mutex> lock(pool_mutex);
  std::byte*                  byte_ptr = reinterpret_cast<std::byte*>(ptr);

  // Validate that the pointer is within the pool area.
  assert(byte_ptr >= pool_data.get() && byte_ptr < pool_data.get() + block_size * block_count &&
         "Trying to deallocate a pointer not in the memory pool");

  // Add the memory block to the free list
  // The memory block is reinterpret_cast to void* to represent the pointer
  // This places the memory block at the front of the free list
  *reinterpret_cast<std::byte**>(byte_ptr) = free_list;
  free_list                                = byte_ptr;
}
