cmake_minimum_required(VERSION 3.12)
project(fcitx5-mcbopomofo VERSION 3.1.1)

set(CMAKE_CXX_STANDARD 20)

find_package(ECM REQUIRED 1.0.0)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

find_package(PkgConfig REQUIRED)
find_package(Fcitx5Core REQUIRED)
find_package(Fcitx5Utils REQUIRED)
find_package(Fcitx5Module REQUIRED COMPONENTS Notifications)
find_package(fmt REQUIRED)
find_package(Gettext REQUIRED)

pkg_check_modules(JSONC REQUIRED IMPORTED_TARGET "json-c")

include(FeatureSummary)
include(GNUInstallDirs)
include(ECMSetupVersion)
include(ECMUninstallTarget)

option(ENABLE_TEST "Build Test" On)

# The vocabulary manager, a Qt 6 program.
option(ENABLE_VOCABULARY_MANAGER "Build the vocabulary manager (needs Qt 6 Widgets, DBus and Network)" On)
if (ENABLE_VOCABULARY_MANAGER)
    find_package(Qt6 QUIET COMPONENTS Widgets DBus Network)
    if (NOT Qt6Widgets_FOUND OR NOT Qt6DBus_FOUND OR NOT Qt6Network_FOUND)
        message(STATUS "Qt 6 Widgets, DBus or Network not found; the vocabulary manager is not built")
        set(ENABLE_VOCABULARY_MANAGER Off)
    else ()
        message(STATUS "Building the vocabulary manager with Qt ${Qt6_VERSION}")
    endif ()
endif ()

# clang-tidy
option(ENABLE_CLANG_TIDY "Enable clang-tidy" Off)
if (ENABLE_CLANG_TIDY)
    MESSAGE(STATUS "Enabled clang-tidy")
    find_program(CLANG_TIDY NAMES "clang-tidy" REQUIRED)
    MESSAGE(STATUS "Found clang-tidy: ${CLANG_TIDY}")
    string(CONCAT CLANG_TIDY_CHECKS "-checks=-*,"
        "clang-analyzer-*,bugprone-*,cert-*,readability-*,"
        "clang-diagnostic-*,"
        "cppcoreguidelines-explicit-virtual-functions,"
        "cppcoreguidelines-virtual-class-destructor,"
        "-bugprone-easily-swappable-parameters,"
        "-readability-const-return-type,"
        "-readability-convert-member-functions-to-static,"
        "-readability-function-cognitive-complexity,"
        "-readability-identifier-length,"
        "-readability-implicit-bool-conversion,"
        "-readability-isolate-declaration,"
        "-readability-suspicious-call-argument"
    )
    set(CLANG_TIDY_COMMAND "${CLANG_TIDY}" "${CLANG_TIDY_CHECKS}")
    # one may wants to use the following instead to focus on a specific category
    # set(CLANG_TIDY_COMMAND "${CLANG_TIDY}" "-checks=-*,clang-analyzer-*")
endif ()

# Setup some compiler option that is generally useful and compatible with Fcitx 5 (C++17)
include("${FCITX_INSTALL_CMAKECONFIG_DIR}/Fcitx5Utils/Fcitx5CompilerSettings.cmake")

# So that `make install` does not also install Google Test
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

# The package version, also shown in the user guide.
set(MCBOPOMOFO_PACKAGE_VERSION "${PROJECT_VERSION}-learning.3")
# The user guide, opened from the input method menu.
set(MCBOPOMOFO_USER_GUIDE_DIR "${CMAKE_INSTALL_DATADIR}/fcitx5-mcbopomofo")
set(MCBOPOMOFO_USER_GUIDE_PATH "${CMAKE_INSTALL_FULL_DATADIR}/fcitx5-mcbopomofo/user-guide.html")

add_subdirectory(src)
add_subdirectory(data)
add_subdirectory(po)

message(STATUS "CMAKE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX}")
message(STATUS "CMAKE_INSTALL_DATADIR ${CMAKE_INSTALL_DATADIR}")

# McBopomofo data
configure_file(data/data.txt mcbopomofo-data.txt)
configure_file(data/data-plain-bpmf.txt mcbopomofo-data-plain-bpmf.txt)
configure_file(data/add-phrase-hook.sh mcbopomofo-add-phrase-hook.sh)
configure_file(data/associated-phrases-v2.txt mcbopomofo-associated-phrases-v2.txt)
configure_file(data/dictionary_service.json mcbopomofo-dictionary-service.json)
configure_file(data/bpmfvs-pua.txt mcbopomofo-bpmfvs-pua.txt)
configure_file(data/bpmfvs-variants.txt mcbopomofo-bpmfvs-variants.txt)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/mcbopomofo-data.txt" DESTINATION "${FCITX_INSTALL_PKGDATADIR}/data")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/mcbopomofo-data-plain-bpmf.txt" DESTINATION "${FCITX_INSTALL_PKGDATADIR}/data")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/mcbopomofo-add-phrase-hook.sh" DESTINATION "${FCITX_INSTALL_PKGDATADIR}/data")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/mcbopomofo-associated-phrases-v2.txt" DESTINATION "${FCITX_INSTALL_PKGDATADIR}/data")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/mcbopomofo-dictionary-service.json" DESTINATION "${FCITX_INSTALL_PKGDATADIR}/data")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/mcbopomofo-bpmfvs-pua.txt" DESTINATION "${FCITX_INSTALL_PKGDATADIR}/data")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/mcbopomofo-bpmfvs-variants.txt" DESTINATION "${FCITX_INSTALL_PKGDATADIR}/data")
if (ENABLE_VOCABULARY_MANAGER)
    install(FILES data/mcbopomofo-vocabulary.desktop DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
endif ()

fcitx5_translate_desktop_file(org.fcitx.Fcitx5.Addon.McBopomofo.metainfo.xml.in
                              org.fcitx.Fcitx5.Addon.McBopomofo.metainfo.xml XML
                              PO_DIRECTORY "${CMAKE_SOURCE_DIR}/po")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/org.fcitx.Fcitx5.Addon.McBopomofo.metainfo.xml" DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
# Debian package (Ubuntu 24.04 / Linux Mint 22)
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "fcitx5-mcbopomofo-smart-learning")
set(CPACK_PACKAGE_VERSION "${MCBOPOMOFO_PACKAGE_VERSION}")
set(CPACK_PACKAGE_CONTACT "McBopomofo Smart Learning Edition")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "McBopomofo Smart Learning Edition for fcitx5")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "fcitx5, wamerican")
if (ENABLE_VOCABULARY_MANAGER)
    string(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS ", libqt6widgets6, libqt6dbus6, libqt6network6")
endif ()
set(CPACK_DEBIAN_PACKAGE_CONFLICTS "fcitx5-mcbopomofo")
set(CPACK_DEBIAN_PACKAGE_REPLACES "fcitx5-mcbopomofo")
set(CPACK_DEBIAN_PACKAGE_PROVIDES "fcitx5-mcbopomofo")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
include(CPack)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

if (ENABLE_TEST)
    enable_testing()
endif ()
