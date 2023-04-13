cd build
cmake ..
make

echo "Running byteArrayTest"
./tests/byteArrayTest

echo "Running clusterTest"
./tests/clusterTest

echo "Running commandHandlerTest"
./tests/instructionHandlerTest

echo "Running keyValueStoreTest"
./tests/keyValueStoreTest

echo "Running networkingTest"
./tests/networkingTest

echo "Running protocolHandlerTest"
./tests/protocolHandlerTest

echo "Running clientTest"
./tests/clientTest