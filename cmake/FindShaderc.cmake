find_path(SHADERC_INCLUDE_DIR NAMES shaderc/shaderc.hpp)
find_library(SHADERC_LIBRARY NAMES shaderc_shared shaderc)

if(SHADERC_INCLUDE_DIR AND SHADERC_LIBRARY)
    add_library(shaderc::shaderc UNKNOWN IMPORTED)
    set_target_properties(shaderc::shaderc PROPERTIES
        IMPORTED_LOCATION "${SHADERC_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SHADERC_INCLUDE_DIR}"
    )
    set(Shaderc_FOUND TRUE)
endif()
