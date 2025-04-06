add_executable(test_msg test_msg.cpp)
target_link_libraries(test_msg PRIVATE protocore gtest gtest_main)

add_test(NAME TestMsgs COMMAND test_msg)

add_executable(test_mempool test_memory_pool.cpp)
target_link_libraries(test_mempool PRIVATE protocore gtest gtest_main)

add_test(NAME TestMemoryPool COMMAND test_mempool)

add_executable(test_queue test_queue.cpp)
target_link_libraries(test_queue PRIVATE protocore gtest gtest_main)

add_test(NAME TestMessageQueue COMMAND test_queue)

add_executable(test_broker test_broker.cpp)
target_link_libraries(test_broker PRIVATE protocore gtest gtest_main)

add_test(NAME TestBroker COMMAND test_broker)

add_executable(test_task test_task.cpp)
target_link_libraries(test_task PRIVATE protocore gtest gtest_main)

add_test(NAME TestTask COMMAND test_task)

add_executable(test_state_manager test_state_manager.cpp)
target_link_libraries(test_state_manager PRIVATE protocore gtest gtest_main)

add_test(NAME testStateManager COMMAND test_state_manager)

add_executable(test_heartbeat test_heartbeat.cpp)
target_link_libraries(test_heartbeat PRIVATE protocore gtest gtest_main)

add_test(NAME testHeartBeat COMMAND test_heartbeat)
