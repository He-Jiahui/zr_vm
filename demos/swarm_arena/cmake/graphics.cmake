# Graphics are optional and local to the demo. A missing development package
# must leave the null build usable instead of entering GLFW's fatal checks.
option(ZR_SWARM_GL "Build the optional Linux X11/OpenGL 3.3 demo backend" ON)
set(ZR_SWARM_DEPS_PREFIX "${PROJECT_SOURCE_DIR}/build/local-tools/debs-extracted/usr"
    CACHE PATH "Prefix containing privately extracted X11/GL development packages")
set(swarm_has_gl FALSE)
set(swarm_headers_ok FALSE)
set(swarm_gl_reason "disabled with ZR_SWARM_GL=OFF")
function(swarm_check_graphics_headers result)
    include(CheckCSourceCompiles)
    set(CMAKE_REQUIRED_INCLUDES "${X11_X11_INCLUDE_PATH};${X11_Xrandr_INCLUDE_PATH};${X11_Xinerama_INCLUDE_PATH};${X11_Xkb_INCLUDE_PATH};${X11_Xcursor_INCLUDE_PATH};${X11_Xi_INCLUDE_PATH};${X11_Xshape_INCLUDE_PATH};${swarm_gl_include}")
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    # Private extracted prefixes can change between configure runs. Check the
    # transitive protocol headers too, rather than caching a stale path result.
    unset(swarm_graphics_headers_compile CACHE)
    check_c_source_compiles("#include <X11/Xlib.h>
        #include <X11/XKBlib.h>
        #include <X11/Xcursor/Xcursor.h>
        #include <X11/extensions/Xrandr.h>
        #include <X11/extensions/Xinerama.h>
        #include <X11/extensions/XInput2.h>
        #include <X11/extensions/shape.h>
        #include <GL/gl.h>
        int main(void) { return 0; }" swarm_graphics_headers_compile)
    set(${result} ${swarm_graphics_headers_compile} PARENT_SCOPE)
endfunction()
if (ZR_SWARM_GL)
    set(swarm_gl_reason "Linux X11, OpenGL headers/libraries, GLFW or glad unavailable")
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        if (IS_DIRECTORY "${ZR_SWARM_DEPS_PREFIX}")
            list(PREPEND CMAKE_PREFIX_PATH "${ZR_SWARM_DEPS_PREFIX}")
            list(PREPEND CMAKE_LIBRARY_PATH "${ZR_SWARM_DEPS_PREFIX}/lib/${CMAKE_LIBRARY_ARCHITECTURE}")
        endif()
        find_package(X11 QUIET)
        find_path(swarm_gl_include GL/gl.h)
        find_library(swarm_gl_library NAMES GL OpenGL libGL.so.1)
        if (X11_FOUND AND X11_Xrandr_INCLUDE_PATH AND X11_Xinerama_INCLUDE_PATH
                AND X11_Xkb_INCLUDE_PATH AND X11_Xcursor_INCLUDE_PATH AND X11_Xi_INCLUDE_PATH
                AND X11_Xshape_INCLUDE_PATH AND swarm_gl_include AND swarm_gl_library
                AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw/CMakeLists.txt"
                AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/src/gl.c"
                AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include/glad/gl.h"
                AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include/KHR/khrplatform.h")
            swarm_check_graphics_headers(swarm_headers_ok)
        endif()
        if (swarm_headers_ok)
            set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
            set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
            set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
            set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
            set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
            set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
            set(GLFW_LIBRARY_TYPE STATIC CACHE STRING "" FORCE)
            add_subdirectory(third_party/glfw EXCLUDE_FROM_ALL)
            add_library(zrgame_glad STATIC third_party/glad/src/gl.c)
            target_include_directories(zrgame_glad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include")
            set(swarm_has_gl TRUE)
            set(swarm_gl_reason "available")
        endif()
    endif()
endif()
message(STATUS "Swarm GL3.3: ${swarm_gl_reason}")
