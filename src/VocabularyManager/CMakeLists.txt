# The vocabulary manager: a Qt program that edits the user's vocabulary files.
# It uses the shared SmartLearning module for every file and never includes
# an fcitx header or links an fcitx library.

# The part without Qt, tested with GoogleTest.
add_library(VocabularyManagerCore STATIC
        FileVocabulary.h
        FileVocabulary.cpp
        LearnedVocabulary.h
        LearnedVocabulary.cpp
        ManagerSettings.h
        ManagerSettings.cpp)
target_include_directories(VocabularyManagerCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(VocabularyManagerCore PUBLIC SmartLearningLib)

if (ENABLE_TEST)
        add_executable(VocabularyManagerTest VocabularyManagerTest.cpp
                LearnedVocabularyTest.cpp)
        target_link_libraries(VocabularyManagerTest GTest::gtest_main VocabularyManagerCore)
        target_compile_definitions(VocabularyManagerTest PRIVATE
                MCBOPOMOFO_TEST_FULL_DATA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../../data/data.txt")
        include(GoogleTest)
        gtest_discover_tests(VocabularyManagerTest)

        # Like the SmartLearning module, the program stays free of fcitx.
        add_test(NAME VocabularyManagerNoFcitxInclude
                COMMAND ${CMAKE_COMMAND}
                        -DSMART_LEARNING_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                        -P ${CMAKE_CURRENT_SOURCE_DIR}/../SmartLearning/NoFcitxIncludeTest.cmake)
endif ()

if (ENABLE_VOCABULARY_MANAGER)
        set(CMAKE_AUTOMOC ON)
        add_executable(mcbopomofo-vocabulary
                main.cpp
                MainWindow.h
                MainWindow.cpp
                Translation.h
                Translation.cpp)
        target_link_libraries(mcbopomofo-vocabulary PRIVATE
                VocabularyManagerCore Qt6::Widgets Qt6::DBus Qt6::Network)
        target_compile_definitions(mcbopomofo-vocabulary PRIVATE
                MCBOPOMOFO_GETTEXT_DOMAIN="fcitx5-mcbopomofo"
                MCBOPOMOFO_LOCALEDIR="${FCITX_INSTALL_LOCALEDIR}"
                MCBOPOMOFO_PKGDATADIR="${FCITX_INSTALL_PKGDATADIR}")
        install(TARGETS mcbopomofo-vocabulary DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif ()
