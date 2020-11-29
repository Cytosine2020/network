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

add_executable(icmp-ping
        src/icmp_ping.cpp src/wire.cpp src/queue.cpp src/athernet.cpp
        src/device/unix_socket.cpp src/server/icmp_server.cpp)
target_link_libraries(icmp-ping pthread ${PCAP_LIBRARY} ${LIBNET_LIBRARY})

add_executable(fake-athernet
        src/fake_athernet.cpp src/wire.cpp src/queue.cpp src/athernet.cpp
        src/device/athernet_socket.cpp src/server/icmp_server.cpp)
target_link_libraries(fake-athernet pthread ${PCAP_LIBRARY} ${LIBNET_LIBRARY})

add_executable(udp-send
        src/udp_send.cpp src/wire.cpp src/queue.cpp src/athernet.cpp
        src/device/athernet_socket.cpp src/server/udp_server.cpp)
target_link_libraries(udp-send pthread ${PCAP_LIBRARY} ${LIBNET_LIBRARY})

add_executable(nat-server
        src/nat.cpp src/wire.cpp src/queue.cpp src/athernet.cpp
        src/device/raw_socket.cpp src/device/unix_socket.cpp src/device/athernet_socket.cpp
        src/server/nat_server.cpp src/server/udp_server.cpp src/server/icmp_server.cpp)
target_link_libraries(nat-server pthread ${PCAP_LIBRARY} ${LIBNET_LIBRARY})

if (APPLE)
    add_custom_command(
            TARGET nat-server
            POST_BUILD
            COMMAND chmod 700 nat-server
            VERBATIM)
elseif (UNIX)
    add_custom_command(
            TARGET nat-server
            POST_BUILD
            COMMAND chmod 700 nat-server && sudo setcap cap_net_admin,cap_net_raw=eip nat-server
            COMMENT "Grant access for raw socket"
            VERBATIM)
endif ()
