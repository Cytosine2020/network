cmake_minimum_required(VERSION 3.0)

project(CS120)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -fno-exceptions -fno-rtti")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb -g3 -fno-omit-frame-pointer -D __DEBUG__")

find_package(PCAP REQUIRED)
find_package(LIBNET REQUIRED)

include_directories(${PCAP_INCLUDE_DIR} ${LIBNET_INCLUDE_DIR} include)

add_executable(icmp
        src/icmp.cpp src/wire/wire.cpp src/device/athernet.cpp
        src/device/unix_socket.cpp src/device/athernet_socket.cpp src/server/icmp_server.cpp)
target_link_libraries(icmp pthread)

add_executable(udp
        src/udp.cpp src/wire/wire.cpp src/device/athernet.cpp
        src/device/unix_socket.cpp src/device/athernet_socket.cpp src/server/udp_server.cpp)
target_link_libraries(udp pthread)

add_executable(tcp
        src/tcp.cpp src/wire/wire.cpp src/device/athernet.cpp
        src/device/raw_socket.cpp src/server/tcp_server.cpp)
target_link_libraries(tcp pthread ${PCAP_LIBRARY} ${LIBNET_LIBRARY})

add_executable(nat
        src/nat.cpp src/wire/wire.cpp  src/wire/get_local_ip.cpp src/device/athernet.cpp
        src/device/raw_socket.cpp src/device/unix_socket.cpp src/server/nat_server.cpp)
target_link_libraries(nat pthread ${PCAP_LIBRARY} ${LIBNET_LIBRARY})

foreach (executable tcp nat)
    if (APPLE)
        add_custom_command(
                TARGET ${executable}
                POST_BUILD
                COMMAND chmod 700 ${executable}
                VERBATIM)
    elseif (UNIX)
        add_custom_command(
                TARGET ${executable}
                POST_BUILD
                COMMAND chmod 700 ${executable} && sudo setcap cap_net_admin,cap_net_raw=eip ${executable}
                COMMENT "Grant access for ${executable}"
                VERBATIM)
    endif ()
endforeach (executable)
