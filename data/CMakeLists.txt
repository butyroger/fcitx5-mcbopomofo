foreach(size 16 22 24 32 64)
    install(DIRECTORY ${size}x${size} DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor
        PATTERN .* EXCLUDE
        PATTERN *~ EXCLUDE)
endforeach(size 16 22 24 32 64)

# The user guide, with the package version in its about section.
configure_file(user-guide.html user-guide.html @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/user-guide.html" DESTINATION "${MCBOPOMOFO_USER_GUIDE_DIR}")

# Candidate panel themes for the classic user interface. They are chosen by
# the user in its settings; the input method never changes them.
foreach(theme mcbopomofo-light mcbopomofo-dark)
    install(FILES themes/${theme}/theme.conf DESTINATION "${FCITX_INSTALL_PKGDATADIR}/themes/${theme}")
endforeach(theme)
