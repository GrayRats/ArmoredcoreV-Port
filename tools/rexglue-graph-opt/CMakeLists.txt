cmake_minimum_required(VERSION 3.25)
project(acv_rexglue_graph_experiment LANGUAGES CXX)
find_package(rexglue 0.10.0 EXACT REQUIRED CONFIG)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
get_filename_component(EXPERIMENT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../tmp/rexglue-graph-opt" ABSOLUTE)
foreach(variant baseline indexed)
  add_executable(graph_${variant} graph_probe.cpp
    "${EXPERIMENT_ROOT}/${variant}/src/codegen/function_graph.cpp")
  target_include_directories(graph_${variant} PRIVATE
    "${EXPERIMENT_ROOT}/${variant}/include"
    "${EXPERIMENT_ROOT}/baseline/include"
    "${EXPERIMENT_ROOT}/baseline/src/codegen"
    "${EXPERIMENT_ROOT}/baseline/thirdparty/disasm")
  target_link_libraries(graph_${variant} PRIVATE rex::runtime)
  target_compile_options(graph_${variant} PRIVATE -O2 -msse4.1 -ffunction-sections -fdata-sections
    -fsanitize=undefined -fno-sanitize-recover=all)
  target_link_options(graph_${variant} PRIVATE -Wl,--gc-sections
    -fsanitize=undefined -fno-sanitize-recover=all)
endforeach()

option(BUILD_INDEXED_CLI "Build the isolated SDK CLI experiment" OFF)
if(BUILD_INDEXED_CLI)
  get_filename_component(ACV_SDK_PREFIX "${rexglue_DIR}/../../.." ABSOLUTE)
  find_library(ACV_DISASM_ARCHIVE NAMES disasm HINTS "${ACV_SDK_PREFIX}/lib" NO_DEFAULT_PATH REQUIRED)
  find_library(ACV_XXHASH_ARCHIVE NAMES xxhash HINTS "${ACV_SDK_PREFIX}/lib" NO_DEFAULT_PATH REQUIRED)
  file(READ "${EXPERIMENT_ROOT}/dependencies.json" DEPS_JSON)
  string(JSON CLI11_ROOT GET "${DEPS_JSON}" cli11 root)
  string(JSON INJA_ROOT GET "${DEPS_JSON}" inja root)
  set(REXGLUE_ROOT "${EXPERIMENT_ROOT}/baseline")
  set(CODEGEN_ROOT "${REXGLUE_ROOT}/src/codegen")
  set(TEMPLATE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/templates")
  execute_process(COMMAND "${CMAKE_COMMAND}"
    "-DTEMPLATE_SOURCE_DIR=${REXGLUE_ROOT}/resources/templates"
    "-DTEMPLATE_OUTPUT_DIR=${TEMPLATE_OUTPUT}"
    -P "${REXGLUE_ROOT}/cmake/embed_templates.cmake"
    COMMAND_ERROR_IS_FATAL ANY)
  set(CORE_FILES codegen codegen_flags analyze code_emitter decoded_binary codegen_writer
    output_partition output_stamp config function_scanner sig_scanner vtable_scanner
    binary_view instruction_dispatch test_support codegen_context phase_register phase_scan
    phase_discover phase_gapfill phase_merge phase_validate template_registry manifest project_recompiler
    ppc/opcode ppc/instruction ppc/disasm builders/context builders/comparison builders/arithmetic
    builders/logical builders/control_flow builders/floating_point builders/memory builders/system builders/vector)
  set(CORE_SOURCES "${EXPERIMENT_ROOT}/indexed/src/codegen/function_graph.cpp")
  foreach(file IN LISTS CORE_FILES)
    # Prefer an isolated, documented replacement in indexed/ over the release copy.
    if(EXISTS "${EXPERIMENT_ROOT}/indexed/src/codegen/${file}.cpp")
      list(APPEND CORE_SOURCES "${EXPERIMENT_ROOT}/indexed/src/codegen/${file}.cpp")
    else()
      list(APPEND CORE_SOURCES "${CODEGEN_ROOT}/${file}.cpp")
    endif()
  endforeach()
  add_library(codegen_indexed STATIC ${CORE_SOURCES})
  target_include_directories(codegen_indexed PUBLIC
    "${EXPERIMENT_ROOT}/indexed/include" "${REXGLUE_ROOT}/include"
    "${INJA_ROOT}/single_include" "${INJA_ROOT}/third_party/include"
    PRIVATE "${CODEGEN_ROOT}" "${CODEGEN_ROOT}/builders"
    "${REXGLUE_ROOT}/thirdparty/disasm" "${TEMPLATE_OUTPUT}")
  target_link_libraries(codegen_indexed PUBLIC rex::runtime fmt::fmt spdlog::spdlog
    PRIVATE "${ACV_DISASM_ARCHIVE}" "${ACV_XXHASH_ARCHIVE}")
  target_compile_options(codegen_indexed PRIVATE -O2 -msse4.1)
  target_precompile_headers(codegen_indexed PRIVATE <rex/codegen/function_types.h>)
  set(CLI_ROOT "${REXGLUE_ROOT}/src/rexglue")
  add_executable(rexglue_indexed "${CLI_ROOT}/main.cpp" "${CLI_ROOT}/ui/ui.cpp"
    "${CLI_ROOT}/ui/progress.cpp" "${CLI_ROOT}/commands/codegen_command.cpp"
    "${CLI_ROOT}/commands/init_command.cpp" "${CLI_ROOT}/commands/test_recompiler.cpp"
    "${CLI_ROOT}/commands/project_scan.cpp")
  target_include_directories(rexglue_indexed PRIVATE "${REXGLUE_ROOT}/src"
    "${REXGLUE_ROOT}/thirdparty/disasm" "${CLI11_ROOT}/include")
  target_link_libraries(rexglue_indexed PRIVATE codegen_indexed)
  # Preserve the already verified executable until the GapFill differential passes.
  set_target_properties(rexglue_indexed PROPERTIES OUTPUT_NAME rexglue_indexed_gapfill)
  target_compile_options(rexglue_indexed PRIVATE -O2 -msse4.1)
  target_precompile_headers(rexglue_indexed PRIVATE <CLI/CLI.hpp>)
endif()
