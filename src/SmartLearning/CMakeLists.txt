cmake_minimum_required(VERSION 3.12)
project(SmartLearningLib)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

add_library(SmartLearningLib
        AtomicFile.h
        AtomicFile.cpp
        BopomofoKeys.h
        BopomofoKeys.cpp
        CandidateAdjustment.h
        CandidateAdjustment.cpp
        EnglishWords.h
        EnglishWords.cpp
        LearnPhrase.h
        LearnPhrase.cpp
        LearningPaths.h
        LearningPaths.cpp
        MixedInput.h
        MixedInput.cpp
        RemovedEntries.h
        RemovedEntries.cpp
        TemporaryPhrases.h
        TemporaryPhrases.cpp
        UserOverrideStore.h
        UserDataFolder.h
        UserDataFolder.cpp
        UserOverrideStore.cpp
        UserPhraseFile.h
        UserPhraseFile.cpp)
target_include_directories(SmartLearningLib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../Engine)
target_link_libraries(SmartLearningLib PUBLIC McBopomofoLMLib gramambular2_lib MandarinLib)

if (ENABLE_TEST)
        enable_testing()

        add_executable(SmartLearningLibTest
                AtomicFileTest.cpp
                BopomofoKeysTest.cpp
                CandidateAdjustmentTest.cpp
                EnglishWordsTest.cpp
                LearnPhraseTest.cpp
                LearningPathsTest.cpp
                MixedInputTest.cpp
                RemovedEntriesTest.cpp
                TemporaryPhrasesTest.cpp
                UserDataFolderTest.cpp
                UserOverrideStoreTest.cpp
                UserPhraseFileTest.cpp)
        target_link_libraries(SmartLearningLibTest GTest::gtest_main SmartLearningLib)
        include(GoogleTest)
        gtest_discover_tests(SmartLearningLibTest)

        # The module must stay free of any fcitx dependency.
        add_test(NAME SmartLearningNoFcitxInclude
                COMMAND ${CMAKE_COMMAND}
                        -DSMART_LEARNING_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                        -P ${CMAKE_CURRENT_SOURCE_DIR}/NoFcitxIncludeTest.cmake)
endif ()
