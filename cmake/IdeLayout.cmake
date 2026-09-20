# IDE presentation only: keep target names, output paths and build dependencies.
# Include after all application, CTest and CPack targets have been defined.
get_property(devtools_ide_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(ide_target IN LISTS devtools_ide_targets)
  get_target_property(ide_target_type "${ide_target}" TYPE)
  if(ide_target MATCHES "^(Continuous|Experimental|Nightly)")
    set_property(TARGET "${ide_target}" PROPERTY FOLDER "CMakeTargets/CTestDashboard")
  elseif(ide_target MATCHES "^devtools-stage-")
    set_property(TARGET "${ide_target}" PROPERTY FOLDER "CMakeTargets/staging")
  elseif(ide_target MATCHES "(^|-)tests?($|-)")
    set_property(TARGET "${ide_target}" PROPERTY FOLDER tests)
  elseif(ide_target_type STREQUAL "STATIC_LIBRARY")
    set_property(TARGET "${ide_target}" PROPERTY FOLDER libraries)
  else()
    set_property(TARGET "${ide_target}" PROPERTY FOLDER projects)
  endif()
endforeach()
if(TARGET libwxui)
  set_property(TARGET libwxui PROPERTY FOLDER 3rdparty)
endif()
if(NOT DEVTOOLS_BUILD_GUI)
  set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT sovkit-console)
endif()

# Browse the supplied SDK contract in its own project; it is not built here.
add_custom_target(sovkit-sdk SOURCES "${SOVKIT_SDK_HEADER}" "${SOVKIT_SDK_DOCUMENTATION}")
set_property(TARGET sovkit-sdk PROPERTY FOLDER 3rdparty)
source_group(include FILES "${SOVKIT_SDK_HEADER}")
source_group(docs FILES "${SOVKIT_SDK_DOCUMENTATION}")

target_sources(devtools_core PRIVATE src/sdk.hpp src/catalogue.hpp)
set(devtools_api_generated)
foreach(api_part count dispatch load members names)
  list(APPEND devtools_api_generated "${generated}/api_${api_part}.inc")
endforeach()
set_source_files_properties(${devtools_api_generated} PROPERTIES HEADER_FILE_ONLY TRUE)
target_sources(devtools_core PRIVATE ${devtools_api_generated})
source_group(generated FILES ${devtools_api_generated} "${generated}/assets.hpp")

# Auxiliary files are visible/editable, never compiled or copied to the release.
file(GLOB_RECURSE devtools_ide_auxiliary CONFIGURE_DEPENDS LIST_DIRECTORIES false
  "${PROJECT_SOURCE_DIR}/res/*"
  "${PROJECT_SOURCE_DIR}/docs/*"
  "${PROJECT_SOURCE_DIR}/cmake/*.cmake"
  "${PROJECT_SOURCE_DIR}/scripts/*.py"
  "${PROJECT_SOURCE_DIR}/scripts/*.bat"
  "${PROJECT_SOURCE_DIR}/.vscode/*.json"
  "${PROJECT_SOURCE_DIR}/.vscode/*.md")
set(devtools_ide_tree_files ${devtools_ide_auxiliary})
list(APPEND devtools_ide_auxiliary
  "${PROJECT_SOURCE_DIR}/CMakeLists.txt"
  "${PROJECT_SOURCE_DIR}/CMakePresets.json"
  "${PROJECT_SOURCE_DIR}/.clang-format"
  "${PROJECT_SOURCE_DIR}/vcpkg.json"
  "${PROJECT_SOURCE_DIR}/README.md"
  "${PROJECT_SOURCE_DIR}/LICENSE"
  "${PROJECT_SOURCE_DIR}/NOTICE.md")
set_source_files_properties(${devtools_ide_auxiliary} PROPERTIES HEADER_FILE_ONLY TRUE)
if(TARGET sovkit-devtools)
  target_sources(sovkit-devtools PRIVATE ${devtools_ide_auxiliary} "${generated}/assets.hpp")
else()
  target_sources(sovkit-console PRIVATE ${devtools_ide_auxiliary})
endif()

file(GLOB devtools_ide_sources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*.cpp" "${PROJECT_SOURCE_DIR}/src/*.hpp")
source_group(TREE "${PROJECT_SOURCE_DIR}" FILES ${devtools_ide_sources} ${devtools_ide_tree_files})
source_group(build FILES "${PROJECT_SOURCE_DIR}/CMakeLists.txt"
  "${PROJECT_SOURCE_DIR}/CMakePresets.json" "${PROJECT_SOURCE_DIR}/.clang-format"
  "${PROJECT_SOURCE_DIR}/vcpkg.json")
source_group(docs FILES "${PROJECT_SOURCE_DIR}/README.md"
  "${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_SOURCE_DIR}/NOTICE.md")
source_group(debug REGULAR_EXPRESSION "\\.natvis$")
if(BUILD_TESTING)
  file(GLOB devtools_ide_tests CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/tests/*.py")
  set_source_files_properties(${devtools_ide_tests} PROPERTIES HEADER_FILE_ONLY TRUE)
  target_sources(devtools-core-tests PRIVATE ${devtools_ide_tests})
  source_group(tests FILES tests/core_test.cpp ${devtools_ide_tests}
    3rdparty/libwxui/tests/desktop_test.cc)
endif()
