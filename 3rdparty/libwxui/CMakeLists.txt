# Standalone build of the libwxui snapshot supplied with this repository.
# No OrbitBridge helper macros, directory globals or private libraries.
file(GLOB_RECURSE sources CONFIGURE_DEPENDS src/*.cc)
add_library(libwxui STATIC ${sources})
target_include_directories(libwxui PUBLIC include PRIVATE "${DEVTOOLS_RAPIDJSON_INCLUDE}" "${DEVTOOLS_UTFCPP_INCLUDE}")
target_link_libraries(libwxui PUBLIC devtools_wx fmt::fmt)
target_compile_features(libwxui PUBLIC cxx_std_20)
if(MSVC)
  target_compile_options(libwxui PRIVATE /utf-8 /FS)
endif()
