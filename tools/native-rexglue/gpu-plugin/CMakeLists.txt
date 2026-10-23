cmake_minimum_required(VERSION 3.25)
project(acv_gpu_plugin LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(rexglue CONFIG REQUIRED)
get_filename_component(REXGLUE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../rexglue-sdk" ABSOLUTE)
include("${REXGLUE_ROOT}/cmake/rex_pch.cmake")
add_compile_definitions(REX_PLATFORM_LINUX=1 REX_HAS_VULKAN=1 REXGLUE_BUILD_CONFIG="$<CONFIG>")
add_compile_options(-march=x86-64-v2)
include_directories(BEFORE "${REXGLUE_ROOT}/include")
set(ACV_GLSLANG_HEADERS "" CACHE PATH "Pinned glslang source headers matching installed SDK libraries")
if(NOT EXISTS "${ACV_GLSLANG_HEADERS}/SPIRV/SpvBuilder.h")
  message(FATAL_ERROR "Set ACV_GLSLANG_HEADERS to the SDK-pinned glslang source directory")
endif()
include_directories("${ACV_GLSLANG_HEADERS}")
add_library(rexruntime ALIAS rex::runtime)
add_library(xxHash::xxhash ALIAS rex::xxhash)
add_library(glslang::SPIRV ALIAS rex::SPIRV)
foreach(component glslang MachineIndependent GenericCodeGen OSDependent OGLCompiler)
  add_library(${component} ALIAS rex::${component})
endforeach()
# The installed package already owns rex::gpu-xenos. Reuse the SDK source
# inventory with absolute paths, creating only the private rexgpu-xenos target.
file(READ "${REXGLUE_ROOT}/src/graphics/CMakeLists.txt" gpu_definition)
string(REPLACE "add_library(rex::gpu-xenos ALIAS rexgpu-xenos)" "" gpu_definition "${gpu_definition}")
string(REPLACE "add_library(rexgpu-xenos SHARED"
  "list(TRANSFORM REXGPU_XENOS_SOURCES PREPEND \"${REXGLUE_ROOT}/src/graphics/\")\nadd_library(rexgpu-xenos SHARED"
  gpu_definition "${gpu_definition}")
file(WRITE "${CMAKE_BINARY_DIR}/gpu_definition.cmake" "${gpu_definition}")
include("${CMAKE_BINARY_DIR}/gpu_definition.cmake")
option(ACV_CAPTURE_UPLOADS "Build bounded CPU-origin upload snapshot diagnostic" OFF)
if(ACV_CAPTURE_UPLOADS)
  set(original_shared_memory "${REXGLUE_ROOT}/src/graphics/vulkan/shared_memory.cpp")
  file(READ "${original_shared_memory}" upload_source)
  set(upload_anchor "      std::memcpy(upload_buffer_mapping,\n                  memory().TranslatePhysical(upload_range_start << page_size_log2()),\n                  upload_buffer_size);")
  string(FIND "${upload_source}" "${upload_anchor}" anchor_offset)
  if(anchor_offset LESS 0)
    message(FATAL_ERROR "SDK upload memcpy changed; review diagnostic insertion")
  endif()
  string(REPLACE "${upload_anchor}"
    "${upload_anchor}\n      AcvRecordUpload(upload_buffer_mapping, upload_range_start << page_size_log2(), upload_buffer_size, submission_current, upload_buffer, upload_buffer_offset);"
    upload_source "${upload_source}")
  # Preserve SDK Vulkan feature-macro/header ordering (including beta enums).
  string(REPLACE "#include <rex/ui/vulkan/util.h>"
    "#include <rex/ui/vulkan/util.h>\n#include \"${CMAKE_CURRENT_LIST_DIR}/upload_probe.h\""
    upload_source "${upload_source}")
  set(diagnostic_shared_memory "${CMAKE_BINARY_DIR}/shared_memory_upload_probe.cpp")
  file(WRITE "${diagnostic_shared_memory}" "${upload_source}")
  get_target_property(gpu_sources rexgpu-xenos SOURCES)
  list(REMOVE_ITEM gpu_sources "${original_shared_memory}")
  list(APPEND gpu_sources "${diagnostic_shared_memory}")
  set_property(TARGET rexgpu-xenos PROPERTY SOURCES "${gpu_sources}")
endif()
set_target_properties(rexgpu-xenos PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

option(ACV_CAPTURE_RESOLVES "Build bounded Resolve/CPU-bake correlation diagnostic" OFF)
if(ACV_CAPTURE_RESOLVES)
  set(original_processor "${REXGLUE_ROOT}/src/graphics/vulkan/command_processor.cpp")
  file(READ "${original_processor}" resolve_source)
  set(resolve_anchor "      std::memcpy(destination, readback.mapped_data[read_index], written_length);")
  string(FIND "${resolve_source}" "${resolve_anchor}" resolve_offset)
  if(resolve_offset LESS 0)
    message(FATAL_ERROR "SDK Resolve publication changed; review diagnostic insertion")
  endif()
  string(REPLACE "${resolve_anchor}"
    "${resolve_anchor}\n      AcvTraceResolvePublish(written_address, written_length, readback.submission_written[read_index], submission_completed_, frame_current_, destination);"
    resolve_source "${resolve_source}")
  string(REPLACE "#include <rex/ui/vulkan/util.h>"
    "#include <rex/ui/vulkan/util.h>\n#include \"${CMAKE_CURRENT_LIST_DIR}/resolve_probe.h\""
    resolve_source "${resolve_source}")
  # Relocated processor source includes generated SPIR-V headers relatively.
  string(REPLACE "#include \"../shaders/" "#include \"${REXGLUE_ROOT}/src/graphics/shaders/"
    resolve_source "${resolve_source}")
  set(diagnostic_processor "${CMAKE_BINARY_DIR}/command_processor_resolve_probe.cpp")
  file(WRITE "${diagnostic_processor}" "${resolve_source}")
  get_target_property(resolve_sources rexgpu-xenos SOURCES)
  list(REMOVE_ITEM resolve_sources "${original_processor}")
  list(APPEND resolve_sources "${diagnostic_processor}")
  set_property(TARGET rexgpu-xenos PROPERTY SOURCES "${resolve_sources}")
endif()

option(ACV_DEFER_RESOLVES_TEST "Private leased-range CPU-consumer deferred-readback experiment" OFF)
if(ACV_DEFER_RESOLVES_TEST)
  if(NOT ACV_CAPTURE_RESOLVES)
    message(FATAL_ERROR "Deferred experiment requires ACV_CAPTURE_RESOLVES for evidence")
  endif()
  file(READ "${diagnostic_processor}" deferred_source)
  string(REPLACE "#include \"${CMAKE_CURRENT_LIST_DIR}/resolve_probe.h\""
    "#include \"${CMAKE_CURRENT_LIST_DIR}/resolve_probe.h\"\n#include \"${CMAKE_CURRENT_LIST_DIR}/deferred_resolve_test.h\""
    deferred_source "${deferred_source}")
  set(slot_anchor "  uint32_t readback_size = rex::round_up(written_length, uint32_t(64 * 1024));")
  string(REPLACE "${slot_anchor}"
    "${slot_anchor}\n  if (!AcvRetireDeferredSlot(this, readback.buffers[write_index])) return false;"
    deferred_source "${deferred_source}")
  set(defer_anchor "  const bool use_delayed_sync =")
  string(FIND "${deferred_source}" "${defer_anchor}" defer_offset)
  if(defer_offset LESS 0)
    message(FATAL_ERROR "Resolve tail changed; review deferred experiment")
  endif()
  set(defer_body [=[
  if (!is_scaled && readback_mode == ReadbackResolveMode::kFull &&
      AcvDeferredEligible(written_address, written_length)) {
    const uint64_t ticket = readback.submission_written[write_index];
    const VkDeviceMemory host_memory = readback.memories[write_index];
    const void* mapping = readback.mapped_data[write_index];
    uint8_t* destination = memory_->TranslatePhysical(written_address);
    if (!AcvAddDeferred(this, {readback.buffers[write_index], written_address,
        written_length, ticket, destination, {},
        [this, ticket]() { return AwaitReadbackSubmission(ticket); },
        [=, this]() {
          VkMappedMemoryRange range = {};
          range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
          range.memory = host_memory; range.size = VK_WHOLE_SIZE;
          if (dfn.vkInvalidateMappedMemoryRanges(device, 1, &range) != VK_SUCCESS) return false;
          std::memcpy(destination, mapping, written_length);
          AcvTraceResolvePublish(written_address, written_length, ticket,
              submission_completed_, frame_current_, destination);
          return true;
        }})) return false;
    if (!EndSubmission(false)) return false;
    readback.current_index = 1 - write_index;
    return true;
  }
]=])
  string(REPLACE "${defer_anchor}" "${defer_body}\n${defer_anchor}" deferred_source "${deferred_source}")
  string(REPLACE "void VulkanCommandProcessor::ShutdownContext() {"
    "void VulkanCommandProcessor::ShutdownContext() {\n  AcvFlushDeferred(this, \"shutdown\");\n  AcvResetDeferred(this);"
    deferred_source "${deferred_source}")
  set(evict_anchor "    std::unordered_map<uint64_t, ReadbackBuffer>& buffer_map) {")
  string(REPLACE "${evict_anchor}"
    "${evict_anchor}\n  if (!AcvFlushDeferred(this, \"eviction\")) return;"
    deferred_source "${deferred_source}")
  file(WRITE "${diagnostic_processor}" "${deferred_source}")
endif()
