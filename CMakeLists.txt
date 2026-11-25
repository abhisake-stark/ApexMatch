cmake_minimum_required(VERSION 3.24)
project(ApexMatch VERSION 0.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Release flags
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native")
    # Debug flags with Sanitizers
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address,undefined")
elseif(MSVC)
    # Basic MSVC flags just in case (UBsan is not fully supported, ASan is)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Ob2 /DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi /Ob0 /Od /RTC1 /fsanitize=address")
endif()

include(FetchContent)

# 1. nlohmann/json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# 2. Catch2
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
)
FetchContent_MakeAvailable(Catch2)

# 3. Asio (Standalone)
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-30-2
)
FetchContent_GetProperties(asio)
if(NOT asio_POPULATED)
    FetchContent_Populate(asio)
endif()

# 4. WebSocket++ (header-only wrapper around ASIO)
FetchContent_Declare(
    websocketpp
    GIT_REPOSITORY https://github.com/zaphoyd/websocketpp.git
    GIT_TAG 0.8.2
)
# websocketpp is header-only and its CMakeLists uses deprecated policies
FetchContent_Populate(websocketpp)

# Create the main executable
add_executable(ApexMatch src/main.cpp src/net/tcp_server.cpp src/ws/broadcaster.cpp)

# Include directories
target_include_directories(ApexMatch PRIVATE 
    include 
    ${asio_SOURCE_DIR}/asio/include
    ${websocketpp_SOURCE_DIR}
)
target_compile_definitions(ApexMatch PRIVATE ASIO_STANDALONE _WEBSOCKETPP_CPP11_STL_ _WEBSOCKETPP_CPP11_THREAD_)

if(WIN32)
    target_link_libraries(ApexMatch PRIVATE ws2_32 wsock32)
endif()

# Link libraries
target_link_libraries(ApexMatch PRIVATE 
    nlohmann_json::nlohmann_json
)

# Tests
enable_testing()
add_executable(ApexMatch_tests tests/test_order_book.cpp tests/test_spsc_ring_buffer.cpp)
target_include_directories(ApexMatch_tests PRIVATE include ${asio_SOURCE_DIR}/asio/include ${websocketpp_SOURCE_DIR})
target_link_libraries(ApexMatch_tests PRIVATE Catch2::Catch2WithMain)

add_executable(ApexMatch_net_tests tests/test_spsc_ring_buffer.cpp)
target_include_directories(ApexMatch_net_tests PRIVATE include)
target_link_libraries(ApexMatch_net_tests PRIVATE Catch2::Catch2WithMain)

add_executable(ApexMatch_net_integration_tests tests/test_tcp_server.cpp src/net/tcp_server.cpp)
target_include_directories(ApexMatch_net_integration_tests PRIVATE include ${asio_SOURCE_DIR}/asio/include)
target_compile_definitions(ApexMatch_net_integration_tests PRIVATE ASIO_STANDALONE)
target_link_libraries(ApexMatch_net_integration_tests PRIVATE Catch2::Catch2WithMain)
if(WIN32)
    target_link_libraries(ApexMatch_net_integration_tests PRIVATE ws2_32 wsock32)
endif()

include(Catch)
# WS Tests target
add_executable(ApexMatch_ws_tests tests/test_ws_broadcaster.cpp src/ws/broadcaster.cpp)
target_include_directories(ApexMatch_ws_tests PRIVATE include ${asio_SOURCE_DIR}/asio/include ${websocketpp_SOURCE_DIR})
target_compile_definitions(ApexMatch_ws_tests PRIVATE ASIO_STANDALONE _WEBSOCKETPP_CPP11_STL_ _WEBSOCKETPP_CPP11_THREAD_)
target_link_libraries(ApexMatch_ws_tests PRIVATE Catch2::Catch2WithMain nlohmann_json::nlohmann_json)
if(WIN32)
    target_link_libraries(ApexMatch_ws_tests PRIVATE ws2_32 wsock32)
endif()

add_test(NAME CoreTests COMMAND ApexMatch_tests)
add_test(NAME NetTests COMMAND ApexMatch_net_tests)
add_test(NAME NetIntegrationTests COMMAND ApexMatch_net_integration_tests)
add_test(NAME WsTests COMMAND ApexMatch_ws_tests)

# Benchmark
add_executable(ApexMatch_benchmark tests/benchmark_order_book.cpp)
target_include_directories(ApexMatch_benchmark PRIVATE include)
