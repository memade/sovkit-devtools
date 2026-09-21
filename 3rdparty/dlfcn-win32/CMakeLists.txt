# 使用静态库，不增加发布时需要携带的 DLL。
add_library(dlfcn-win32 STATIC src/dlfcn.c src/dlfcn.h)
target_include_directories(dlfcn-win32 PUBLIC src)
set_target_properties(dlfcn-win32 PROPERTIES FOLDER 3rdparty)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES src/dlfcn.c src/dlfcn.h)
