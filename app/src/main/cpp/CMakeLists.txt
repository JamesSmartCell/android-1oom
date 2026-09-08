cmake_minimum_required(VERSION 3.22.1)
project(oomdroid C)

# Default: the upstream 1oom engine at the repo-root src/ path.
# app/src/main/cpp -> app/src/main -> app/src -> app -> <repo>
if(NOT OOM_SRC)
    set(OOM_SRC "${CMAKE_CURRENT_LIST_DIR}/../../../../src")
endif()

if(NOT EXISTS "${OOM_SRC}/main.c")
    message(FATAL_ERROR "1oom not found at ${OOM_SRC}")
endif()

file(GLOB OOM_CORE "${OOM_SRC}/*.c")
set(_oom_skip lbxview.c lbxedit.c pbxmake.c pbxdump.c saveconv.c gfxconv.c)
foreach(_f IN LISTS OOM_CORE)
    get_filename_component(_n "${_f}" NAME)
    if(_n IN_LIST _oom_skip)
        list(REMOVE_ITEM OOM_CORE "${_f}")
    endif()
endforeach()

file(GLOB OOM_GAME "${OOM_SRC}/game/*.c")
file(GLOB OOM_UI "${OOM_SRC}/ui/classic/*.c")

add_library(oomdroid SHARED
    oom_jni.c
    hw_android.c
    os_android.c
    audio_android.c
    ${OOM_CORE}
    ${OOM_GAME}
    ${OOM_UI}
)

target_include_directories(oomdroid PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${OOM_SRC}
    ${OOM_SRC}/game
    ${OOM_SRC}/ui/classic
)

if(NOT ANDROID_APP_VERSION)
    set(ANDROID_APP_VERSION "dev")
endif()

target_compile_definitions(oomdroid PRIVATE
    HAVE_STRCASECMP=1
    ANDROID_APP_VERSION="${ANDROID_APP_VERSION}"
)

target_compile_options(oomdroid PRIVATE
    -O2
    -Wno-unused-parameter
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-missing-field-initializers
    -Wno-format
    -Wno-pointer-sign
    -Wno-sign-compare
    -Wno-incompatible-pointer-types
)

find_library(log-lib log)
find_library(android-lib android)
find_library(jnigraphics-lib jnigraphics)

target_link_libraries(oomdroid
    ${log-lib}
    ${android-lib}
    ${jnigraphics-lib}
    m
)
