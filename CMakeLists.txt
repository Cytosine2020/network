cmake_minimum_required(VERSION 3.0)
project(CS120)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -fno-exceptions -fno-rtti")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb -g3 -fno-omit-frame-pointer -D __DEBUG__")

find_package(PCAP REQUIRED)
find_package(LIBNET REQUIRED)

include_directories(${PCAP_INCLUDE_DIR} ${LIBNET_INCLUDE_DIR} include)

add_executable(nat-server src/main.cpp src/wire.cpp)
target_link_libraries(nat-server ${PCAP_LIBRARY} ${LIBNET_LIBRARY})

if (APPLE)
    add_custom_command(
            TARGET nat-server
            POST_BUILD
            COMMAND chmod 700 nat-server
            VERBATIM
    )
elseif (UNIX)
    add_custom_command(
            TARGET nat-server
            POST_BUILD
            COMMAND chmod 700 nat-server && sudo setcap cap_net_admin,cap_net_raw=eip nat-server
            COMMENT "Grant access for raw socket"
            VERBATIM
    )
endif ()
