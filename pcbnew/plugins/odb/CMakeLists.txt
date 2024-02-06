
# Sources for the pcbnew PLUGIN called IPC2581_PLUGIN

include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )


set( ODBPP_SRCS
    odb_util.cpp
    odb_attribute.cpp
    odb_feature.cpp
    odb_component.cpp
    odb_netlist.cpp
    odb_eda_data.cpp
    odb_fonts.cpp
    odb_entity.cpp
    odb_plugin.cpp
    )

add_library( odbpp STATIC ${ODBPP_SRCS} )

target_link_libraries( odbpp pcbcommon )
