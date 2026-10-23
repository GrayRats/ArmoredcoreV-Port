# Handwritten optional CLI override; include after generated/rexglue.cmake.
# Installed runtime/ABI remains the pinned release; only codegen CLI is replaced.
set(ACV_CODEGEN_EXECUTABLE "" CACHE FILEPATH "Explicit isolated ReXGlue codegen CLI")
if(ACV_CODEGEN_EXECUTABLE)
    if(NOT EXISTS "${ACV_CODEGEN_EXECUTABLE}" OR NOT TARGET rex::rexglue)
        message(FATAL_ERROR "ACV codegen override requires an existing CLI and rex::rexglue target")
    endif()
    get_target_property(_acv_codegen_imported rex::rexglue IMPORTED)
    if(NOT _acv_codegen_imported)
        message(FATAL_ERROR "ACV codegen override supports installed imported SDK targets only")
    endif()
    set_property(TARGET rex::rexglue PROPERTY IMPORTED_LOCATION "${ACV_CODEGEN_EXECUTABLE}")
    get_target_property(_acv_codegen_configs rex::rexglue IMPORTED_CONFIGURATIONS)
    foreach(_acv_codegen_config IN LISTS _acv_codegen_configs)
        set_property(TARGET rex::rexglue PROPERTY
            IMPORTED_LOCATION_${_acv_codegen_config} "${ACV_CODEGEN_EXECUTABLE}")
    endforeach()
endif()
