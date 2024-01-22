
# Sources for the pcbnew PLUGIN called IPC2581_PLUGIN

include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )


set( ODBPP_SRCS
    odb_plugin.cpp
    odb_attribute.cpp
    odb_entity.cpp
    odb_eda_data.cpp
    odb_feature.cpp
    odb_util.cpp
    )

add_library( odbpp STATIC ${ODB_SRCS} )

target_link_libraries( odbpp pcbcommon )
