add_executable(test_msg test_msg.cpp)
target_link_libraries(test_msg PRIVATE protocore gtest gtest_main)

add_test(NAME TestMsgs COMMAND test_msg)

add_executable(test_mempool test_memory_pool.cpp)
target_link_libraries(test_mempool PRIVATE protocore gtest gtest_main)

add_test(NAME TestMemoryPool COMMAND test_mempool)