set(BYTEARRAY_TEST byteArrayTest)
set(BYTEARRAY_TEST_SRC testByteArray.cpp)

add_executable(${BYTEARRAY_TEST} ${BYTEARRAY_TEST_SRC})

link_directories(${CMAKE_CURRENT_BINARY_DIR}/../src)
target_link_libraries(${BYTEARRAY_TEST} PRIVATE ByteArray_l)
target_include_directories(${BYTEARRAY_TEST} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../src)