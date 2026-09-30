# This directory is copied to Krita's plugins/dockers/baron by install-overlay.py.
set(BARON_SOURCES
    BaronDocker.cpp
    native/BaronPanel.cpp
    native/Credentials.cpp
    native/Localization.cpp
    native/translations.qrc
    native/OrchestrionClient.cpp)
kis_add_library(kritabarondocker MODULE ${BARON_SOURCES})
set_target_properties(kritabarondocker PROPERTIES AUTORCC ON)
target_include_directories(kritabarondocker PRIVATE native)
target_link_libraries(kritabarondocker kritaui Qt5::Network)
if(ANDROID)
    find_package(Qt5 REQUIRED COMPONENTS AndroidExtras)
    target_link_libraries(kritabarondocker Qt5::AndroidExtras)
endif()
install(TARGETS kritabarondocker DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
