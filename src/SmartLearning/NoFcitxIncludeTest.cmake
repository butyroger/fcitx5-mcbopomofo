# Fails if any source in src/SmartLearning includes an fcitx header.
# Usage: cmake -DSMART_LEARNING_DIR=<dir> -P NoFcitxIncludeTest.cmake
if (NOT SMART_LEARNING_DIR)
    get_filename_component(SMART_LEARNING_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
endif ()

file(GLOB_RECURSE sources "${SMART_LEARNING_DIR}/*.h" "${SMART_LEARNING_DIR}/*.cpp")
list(LENGTH sources count)
if (count EQUAL 0)
    message(FATAL_ERROR "No sources found under ${SMART_LEARNING_DIR}")
endif ()

foreach (source IN LISTS sources)
    file(STRINGS "${source}" lines)
    foreach (line IN LISTS lines)
        if (line MATCHES "^[ \t]*#[ \t]*include[ \t]*[<\"]fcitx")
            message(FATAL_ERROR "fcitx include in ${source}: ${line}")
        endif ()
    endforeach ()
endforeach ()
