cmake_minimum_required(VERSION 3.12)
project(SmartLearningLib)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

add_library(SmartLearningLib
        AtomicFile.h
        AtomicFile.cpp
        LearningPaths.h
        LearningPaths.cpp
        UserOverrideStore.h
        UserDataFolder.h
        UserDataFolder.cpp
        UserOverrideStore.cpp)
target_include_directories(SmartLearningLib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../Engine)
target_link_libraries(SmartLearningLib PUBLIC McBopomofoLMLib gramambular2_lib)

if (ENABLE_TEST)
        enable_testing()

        add_executable(SmartLearningLibTest
                AtomicFileTest.cpp
                LearningPathsTest.cpp
                UserDataFolderTest.cpp
                UserOverrideStoreTest.cpp)
        target_link_libraries(SmartLearningLibTest GTest::gtest_main SmartLearningLib)
        include(GoogleTest)
        gtest_discover_tests(SmartLearningLibTest)

        # The module must stay free of any fcitx dependency.
        add_test(NAME SmartLearningNoFcitxInclude
                COMMAND ${CMAKE_COMMAND}
                        -DSMART_LEARNING_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                        -P ${CMAKE_CURRENT_SOURCE_DIR}/NoFcitxIncludeTest.cmake)
endif ()
