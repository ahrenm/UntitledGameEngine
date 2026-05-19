# Invoked by the pack_assets CMake target.
# Variables passed in via -D:
#   SOURCE_DIR  - project source root (contains the assets/ folder)
#   OUTPUT_ZIP  - full path to the output assets.zip

message(STATUS "Packing assets from: ${SOURCE_DIR}/assets")
message(STATUS "Output zip:          ${OUTPUT_ZIP}")

# cmake -E tar respects WORKING_DIRECTORY so entries are stored as
# "assets/..." rather than absolute paths, which is what PhysFS expects.
execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar "cf" "${OUTPUT_ZIP}" --format=zip "assets"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE result
    ERROR_VARIABLE  error_output
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "Failed to create assets.pak: ${error_output}")
endif()

message(STATUS "assets.pak written successfully.")

