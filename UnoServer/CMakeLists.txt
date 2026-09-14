cmake_minimum_required(VERSION 4.0)
project(UnoServer C)

set(CMAKE_C_STANDARD 11)

add_executable(UnoServer
        src/acceptor.c
        src/listener.c
        src/main.c
        src/queue.c
        src/server.c
        src/worker.c
        src/writer.c
)

target_include_directories(UnoServer PRIVATE
        include
)

target_compile_definitions(UnoServer PRIVATE
        _WIN32_WINNT=0x0601
        WIN32_LEAN_AND_MEAN
)

target_link_libraries(UnoServer PRIVATE
        ws2_32
)

### TESTE CLIENTE

add_executable(UnoClientTest
        test/client.c
)

target_include_directories(UnoClientTest PRIVATE
        include
)

target_compile_definitions(UnoClientTest PRIVATE
        _WIN32_WINNT=0x0601
        WIN32_LEAN_AND_MEAN
)

target_link_libraries(UnoClientTest PRIVATE
        ws2_32
)