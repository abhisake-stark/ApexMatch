# CMake generated Testfile for 
# Source directory: C:/Users/ABHISHEK/OneDrive/Desktop/StarkLOB
# Build directory: C:/Users/ABHISHEK/OneDrive/Desktop/StarkLOB/build/release
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test("CoreTests" "C:/Users/ABHISHEK/OneDrive/Desktop/StarkLOB/build/release/StarkLOB_tests.exe")
set_tests_properties("CoreTests" PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/ABHISHEK/OneDrive/Desktop/StarkLOB/CMakeLists.txt;107;add_test;C:/Users/ABHISHEK/OneDrive/Desktop/StarkLOB/CMakeLists.txt;0;")
add_test("NetTests" "C:/Users/ABHISHEK/OneDrive/Desktop/StarkLOB/build/release/StarkLOB_net_tests.exe")
set_tests_properties("NetTests" PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/ABHISHEK/OneDrive/Desktop/StarkLOB/CMakeLists.txt;108;add_test;C:/Users/ABHISHEK/OneDrive/Desktop/StarkLOB/CMakeLists.txt;0;")
add_test("NetIntegrationTests" "C:/Users/ABHISHEK/OneDrive/Desktop/StarkLOB/build/release/StarkLOB_net_integration_tests.exe")
set_tests_properties("NetIntegrationTests" PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/ABHISHEK/OneDrive/Desktop/StarkLOB/CMakeLists.txt;109;add_test;C:/Users/ABHISHEK/OneDrive/Desktop/StarkLOB/CMakeLists.txt;0;")
add_test("WsTests" "C:/Users/ABHISHEK/OneDrive/Desktop/StarkLOB/build/release/StarkLOB_ws_tests.exe")
set_tests_properties("WsTests" PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/ABHISHEK/OneDrive/Desktop/StarkLOB/CMakeLists.txt;110;add_test;C:/Users/ABHISHEK/OneDrive/Desktop/StarkLOB/CMakeLists.txt;0;")
subdirs("_deps/nlohmann_json-build")
subdirs("_deps/catch2-build")
