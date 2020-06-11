cmake_minimum_required (VERSION 2.6)
project (FileTransfer)

aux_source_directory(src DIR_SRC)
aux_source_directory(server DIR_SVR)

include_directories(include src)
LINK_DIRECTORIES("/root/boost_1_58_0/stage/lib/")

add_executable(file_transfer_server ${DIR_SRC} ${DIR_SVR})
add_library(file_transfer STATIC ${DIR_SRC}) 

TARGET_LINK_LIBRARIES(file_transfer_server boost_system.so boost_thread.so boost_filesystem.so)
