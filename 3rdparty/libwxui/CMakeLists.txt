# Standalone build of the libwxui snapshot supplied with this repository.
# No OrbitBridge helper macros, directory globals or private libraries.
file(GLOB_RECURSE sources CONFIGURE_DEPENDS src/*.cc)
file(GLOB_RECURSE headers CONFIGURE_DEPENDS include/*.hpp include/*.h src/*.hpp src/*.h)
add_library(libwxui STATIC ${sources} ${headers})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${sources} ${headers})
source_group(build FILES "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt")
target_include_directories(libwxui PUBLIC include PRIVATE "${DEVTOOLS_RAPIDJSON_INCLUDE}" "${DEVTOOLS_UTFCPP_INCLUDE}")
target_link_libraries(libwxui PUBLIC devtools_wx fmt::fmt)
target_compile_features(libwxui PUBLIC cxx_std_20)
if(MSVC)
  target_compile_options(libwxui PRIVATE /utf-8 /FS)
endif()
