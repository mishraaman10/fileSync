cmake_minimum_required(VERSION 3.10)
project(SyncApp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")

# Include directories
include_directories(
    destination
    source
    sync
    common
)

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Define executable
add_executable(syncApp
    main.cpp
    destination/destination_manager.cpp
    source/source_manager.cpp
    sync/sync_engine.cpp
    sync/client_mode.cpp
    sync/client_session.cpp
    sync/server_mode.cpp
    common/thread_pool.cpp
    common/data_transfer.cpp
)

# Link OpenSSL to the correct target
target_link_libraries(syncApp OpenSSL::SSL OpenSSL::Crypto)
