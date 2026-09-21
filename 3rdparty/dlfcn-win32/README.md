# dlfcn-win32

Source snapshot copied from OrbitBridge/tests/sdk-demo-cpp-1/3rdparty/dlfcn-win32.
Upstream: https://github.com/dlfcn-win32/dlfcn-win32 (MIT; see COPYING).

DevTools builds the library statically on Windows. Other platforms use system dlfcn.

Local change: `dlopen` accepts UTF-8 paths and converts them to UTF-16 before
calling LoadLibraryExW. Absolute paths use LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
LOAD_LIBRARY_SEARCH_DEFAULT_DIRS, preserving the previous DevTools dependency
search policy. This keeps Chinese SDK paths working without changing the process
code page. The dlopen/dlsym/dlerror/dlclose interface is unchanged.
