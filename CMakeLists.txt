cmake_minimum_required(VERSION 3.10)
project(fad)

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -Wextra")

find_package(Threads REQUIRED)
find_package(Boost 1.70 REQUIRED COMPONENTS program_options coroutine context system)

include_directories(${Boost_INCLUDE_DIR})

add_executable(fad src/config.cpp src/fad.cpp src/fanotify.cpp src/log.cpp src/metadataworker.cpp src/pid.cpp)

target_link_libraries(fad ${Boost_LIBRARIES})
target_link_libraries(fad Threads::Threads)

install(TARGETS ${PROJECT_NAME} RUNTIME  DESTINATION bin) 
install(FILES   ${PROJECT_SOURCE_DIR}/cfg/fad.conf    DESTINATION /etc/              COMPONENT configuration)
install(FILES   ${PROJECT_SOURCE_DIR}/cfg/fad.service DESTINATION lib/systemd/system COMPONENT init)
install(CODE    "execute_process(COMMAND systemctl daemon-reload COMMAND systemctl enable fad)")
