# This directory is copied to Krita's plugins/dockers/baron by install-overlay.py.
include(native/BaronVersion.cmake)
baron_read_version("${CMAKE_CURRENT_SOURCE_DIR}/version.json")
set(BARON_SOURCES
    BaronDocker.cpp
    native/PluginUi.cpp native/plugin-icons.qrc native/BaronPanel.cpp
    native/GuidancePanel.cpp
    native/InterfaceSettings.cpp
    native/OrientationLayouts.cpp native/InpaintWidget.cpp
    native/AuthorCredits.cpp
    native/CloudWorkflow.cpp
    native/UpscaleWidget.cpp
    native/ConnectionButton.cpp
    native/PromptLogic.cpp native/PromptActions.cpp native/prompt.qrc
    native/JobQueue.cpp native/HistoryStore.cpp
    native/ModelThumbnails.cpp
    native/ModelCatalog.cpp native/TagModel.cpp native/tags.qrc
    native/BaronUpdates.cpp
    native/BaronDiagnostics.cpp
    native/Credentials.cpp
    native/PersonalFonts.cpp
    native/Localization.cpp
    native/translations.qrc
    native/OrchestrionClient.cpp)
kis_add_library(kritabarondocker MODULE ${BARON_SOURCES})
set_target_properties(kritabarondocker PROPERTIES AUTORCC ON)
target_include_directories(kritabarondocker PRIVATE native)
target_compile_definitions(kritabarondocker PRIVATE BARON_VERSION="${BARON_VERSION}" BARON_VERSION_CODE=${BARON_VERSION_CODE})
find_package(Qt5 REQUIRED COMPONENTS WebSockets Concurrent Qml)
target_link_libraries(kritabarondocker kritaui Qt5::Network Qt5::WebSockets Qt5::Concurrent Qt5::Qml)
if(ANDROID)
    find_package(Qt5 REQUIRED COMPONENTS AndroidExtras)
    target_link_libraries(kritabarondocker Qt5::AndroidExtras)
endif()
install(TARGETS kritabarondocker DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
