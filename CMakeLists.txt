cmake_minimum_required(VERSION 3.8)
project(ISTele)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 99)

include_directories(/usr/local/opt/openssl/include)
link_directories(/usr/local/opt/openssl/lib /usr/local/lib)

set(MAIN client.cpp)
set(SERVER server.cpp)
set(SOURCE_FILES ist_protocol.h SelectDriver.cpp SelectDriver.hpp
        EventDriver.hpp ISTransporter.cpp ISTransporter.hpp rm_net/rm_net.c rm_net/rm_net.h
        Encryption/ISEncryptor.hpp Encryption/ISEncryptor.cpp Encryption/RMRSA.c Encryption/RMRSA.h ISRouter.cpp ISRouter.hpp)
add_executable(ISTele ${MAIN} ${SOURCE_FILES})
add_executable(ISServer ${SERVER} ${SOURCE_FILES})
target_link_libraries(ISTele ssl crypto)
target_link_libraries(ISServer ssl crypto pthread)