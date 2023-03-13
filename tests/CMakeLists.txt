#Function to add test executables
#add_test(execname libraryame sourcefile.cpp othersource.cpp...)
function(add_test test_name test_library)
    add_executable(${test_name} ${ARGN})
    target_link_libraries(${test_name} PRIVATE ${test_library})
endfunction()


#ByteArray Test
add_test(byteArrayTest ByteArray_l ByteArray.test.cpp)

#KeyValueStore Test
add_test(keyValueStoreTest KeyValueStore_l InMemoryKVS.test.cpp)

#Networking Test
add_test(networkingTest Networking_l Net.test.cpp)