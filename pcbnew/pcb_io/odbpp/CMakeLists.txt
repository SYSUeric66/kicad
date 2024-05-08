
# Sources for the pcbnew pcb_io called ODB Plus Plus

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
    pcb_io_odbpp.cpp
    )

add_library( odbpp STATIC ${ODBPP_SRCS} )

target_link_libraries( odbpp pcbcommon )
