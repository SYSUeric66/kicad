
# Sources for the pcbnew PLUGIN called IPC2581_PLUGIN

include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )


set( ODB_SRCS
    odb_plugin.cpp
    )

add_library( odb STATIC ${ODB_SRCS} )

target_link_libraries( odb pcbcommon )
