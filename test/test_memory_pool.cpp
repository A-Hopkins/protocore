#include <gtest/gtest.h>
#include "memory_pool.h"
#include <thread>
#include <vector>
#include <set>
#include <chrono>
#include <algorithm>
#include <atomic>

// Basic functionality tests
TEST(MemoryPoolTest, Initialization)
{
  // Test that the memory pool can be initialized with various sizes
  MemoryPool pool1(64, 10);
  MemoryPool pool2(128, 100);
  MemoryPool pool3(sizeof(void*) + 1, 1); // Minimum block size

  // These shouldn't crash, but we can't directly test internal state
  SUCCEED();
}

TEST(MemoryPoolTest, BasicAllocateAndDeallocate)
{
  const size_t BLOCK_SIZE = 64;
  const size_t BLOCK_COUNT = 10;
  
  MemoryPool pool(BLOCK_SIZE, BLOCK_COUNT);
  
  // Allocate a block and make sure it's not null
  void* block = pool.allocate();
  ASSERT_NE(nullptr, block);
  
  // Write to the block to make sure it's usable
  memset(block, 0xAB, BLOCK_SIZE);
  
  // Deallocate should not crash
  pool.deallocate(block);
}

TEST(MemoryPoolTest, AllocateAllBlocks)
{
  const size_t BLOCK_SIZE = 64;
  const size_t BLOCK_COUNT = 10;
  
  MemoryPool pool(BLOCK_SIZE, BLOCK_COUNT);
  
  // Allocate all blocks
  std::vector<void*> blocks;
  for (size_t i = 0; i < BLOCK_COUNT; ++i)
  {
    void* block = pool.allocate();
    ASSERT_NE(nullptr, block);
    blocks.push_back(block);
  }
  
  // Next allocation should fail (return nullptr)
  void* overflow_block = pool.allocate();
  EXPECT_EQ(nullptr, overflow_block);
  
  // Deallocate all blocks
  for (auto block : blocks)
  {
    pool.deallocate(block);
  }
  
  // Should be able to allocate again
  void* new_block = pool.allocate();
  ASSERT_NE(nullptr, new_block);
  pool.deallocate(new_block);
}

// Test invalid deallocation: pass a pointer that wasn't allocated by the pool.
TEST(MemoryPoolTest, InvalidDeallocation)
{
  const size_t BLOCK_SIZE = 64;
  const size_t BLOCK_COUNT = 5;
  MemoryPool pool(BLOCK_SIZE, BLOCK_COUNT);

  int dummy = 0;
  // Since our pool uses assert() to validate the pointer,
  // deallocating a pointer not in the pool should trigger the assert.
  EXPECT_DEATH(pool.deallocate(&dummy), "Trying to deallocate a pointer not in the memory pool");
}

TEST(MemoryPoolTest, UniqueBlocks)
{
  const size_t BLOCK_SIZE = 64;
  const size_t BLOCK_COUNT = 100;
  
  MemoryPool pool(BLOCK_SIZE, BLOCK_COUNT);
  
  // Allocate many blocks and ensure they're all different addresses
  std::set<void*> block_addresses;
  for (size_t i = 0; i < BLOCK_COUNT; ++i)
  {
    void* block = pool.allocate();
    ASSERT_NE(nullptr, block);
    
    // Each address should be unique
    ASSERT_EQ(block_addresses.count(block), 0) << "Duplicate block address detected";
    block_addresses.insert(block);
  }
}

TEST(MemoryPoolTest, AlignmentAndSize)
{
  // Test with various block sizes
  std::vector<size_t> block_sizes = {16, 32, 64, 128, 256};
  
  for (auto block_size : block_sizes)
  {
    MemoryPool pool(block_size, 10);
    
    // Allocate some blocks
    void* block1 = pool.allocate();
    void* block2 = pool.allocate();
    
    // Check that blocks are the correct distance apart (block_size)
    ptrdiff_t diff = static_cast<char*>(block2) - static_cast<char*>(block1);
    EXPECT_EQ(std::abs(diff), block_size);
    
    // Clean up
    pool.deallocate(block1);
    pool.deallocate(block2);
  }
}

// Test full free list recovery:
// Allocate all blocks, deallocate in a random order, then reallocate all blocks.
TEST(MemoryPoolTest, FullFreeListRecovery)
{
  const size_t BLOCK_SIZE = 64;
  const size_t BLOCK_COUNT = 50;
  MemoryPool pool(BLOCK_SIZE, BLOCK_COUNT);
  
  std::vector<void*> allocated;
  // Allocate all blocks.
  for (size_t i = 0; i < BLOCK_COUNT; ++i)
  {
    void* block = pool.allocate();
    ASSERT_NE(block, nullptr);
    allocated.push_back(block);
  }
  
  // Shuffle deallocation order.
  std::random_shuffle(allocated.begin(), allocated.end());
  for (void* block : allocated)
    pool.deallocate(block);
  
  // Reallocate all blocks and verify non-null pointers.
  std::vector<void*> reallocated;
  for (size_t i = 0; i < BLOCK_COUNT; ++i)
  {
    void* block = pool.allocate();
    ASSERT_NE(block, nullptr);
    reallocated.push_back(block);
  }
}

// Test multithreading: create several threads that allocate and deallocate blocks.
TEST(MemoryPoolTest, ThreadSafety)
{
  const size_t BLOCK_SIZE = 64;
  const size_t BLOCK_COUNT = 1000;
  const size_t ITERATIONS = 10000;
  
  MemoryPool pool(BLOCK_SIZE, BLOCK_COUNT);
  std::atomic<bool> startFlag{false};
  
  auto worker = [&pool, &startFlag, ITERATIONS]() {
    // Wait until all threads are ready.
    while (!startFlag.load());
    for (size_t i = 0; i < ITERATIONS; ++i)
    {
      void* block = pool.allocate();
      if (block) {
        // Simulate work by writing some bytes.
        memset(block, 0xAA, BLOCK_SIZE);
        pool.deallocate(block);
      }
    }
  };

  const size_t NUM_THREADS = 8;
  std::vector<std::thread> threads;
  for (size_t i = 0; i < NUM_THREADS; ++i)
    threads.emplace_back(worker);
  
  startFlag.store(true);
  for (auto &t : threads)
    t.join();
  
  // Additional check: Drain the pool and ensure that exactly BLOCK_COUNT blocks are available.
  std::set<void*> allocatedBlocks;
  void* block = nullptr;
  while ((block = pool.allocate()) != nullptr)
  {
    allocatedBlocks.insert(block);
  }
  EXPECT_EQ(allocatedBlocks.size(), BLOCK_COUNT) << "Pool free list corrupt: Expected " << BLOCK_COUNT << " blocks";
  
  // Optionally deallocate back the blocks to restore pool, if further tests depend on it.
  for (auto blk : allocatedBlocks)
    pool.deallocate(blk);
  
  SUCCEED();
}

TEST(MemoryPoolTest, Performance)
{
  const size_t BLOCK_SIZE = 64;
  const size_t BLOCK_COUNT = 10000;
  const size_t ITERATIONS = 100000;
  
  MemoryPool pool(BLOCK_SIZE, BLOCK_COUNT);
  
  auto start = std::chrono::high_resolution_clock::now();
  
  // Allocate and deallocate in a pattern that simulates real usage
  std::vector<void*> blocks;
  blocks.reserve(BLOCK_COUNT);
  
  for (size_t i = 0; i < ITERATIONS; ++i)
  {
    // Allocate a block
    if (blocks.size() < BLOCK_COUNT / 2 || (rand() % 100 < 70))
    {
      void* block = pool.allocate();
      if (block)
      {
        blocks.push_back(block);
      }
    }
    // Deallocate a block
    else if (!blocks.empty())
    {
      size_t index = rand() % blocks.size();
      pool.deallocate(blocks[index]);
      blocks.erase(blocks.begin() + index);
    }
  }
  
  // Clean up remaining blocks
  for (auto block : blocks)
  {
    pool.deallocate(block);
  }
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  
  std::cout << "Performance test completed in " << duration << "ms" << std::endl;
  // This is an informational test, not a pass/fail
  SUCCEED();
}
