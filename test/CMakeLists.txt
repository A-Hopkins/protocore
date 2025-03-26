add_executable(test_msg test_msg.cpp)
target_link_libraries(test_msg PRIVATE protocore gtest gtest_main)

add_test(NAME TestMsgs COMMAND test_msg)