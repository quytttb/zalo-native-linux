cmake_minimum_required(VERSION 3.24)

project(ZaloNative
    VERSION 1.0.0
    DESCRIPTION "Experimental native Qt 6/QML Zalo client for Linux"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

include(GNUInstallDirs)
include(CTest)

option(ZALO_BUILD_TESTS "Build the test suite" ON)
option(ZALO_ENABLE_SANITIZERS "Build with AddressSanitizer/UBSan (GCC/Clang only)" OFF)
option(ZALO_WEBENGINE "Build the optional historical Qt WebEngine diagnostics and Web wrapper" OFF)
option(ZALO_BUILD_QR_PROBE "Build the archived experimental QR probe (historical reference only)" OFF)

if(ZALO_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network)

set(ZALO_HAVE_WEBENGINE OFF)
if(ZALO_WEBENGINE)
    find_package(Qt6 6.5 QUIET COMPONENTS WebEngineQuick WebChannel)
    if(TARGET Qt6::WebEngineQuick AND TARGET Qt6::WebChannel)
        set(ZALO_HAVE_WEBENGINE ON)
    else()
        message(WARNING
            "Qt WebEngine development files were not found. Building without the "
            "optional renderer host; the native shell and all tools still build. "
            "Pass -DZALO_WEBENGINE=OFF to silence this warning.")
    endif()
endif()

add_subdirectory(src/core)
add_subdirectory(src/backend)
add_subdirectory(src/cli)

if(ZALO_BUILD_QR_PROBE)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
    find_package(OpenSSL REQUIRED COMPONENTS Crypto)
    add_executable(zalo-qr-probe src/probe/main.cpp src/probe/QrProtocol.cpp src/probe/QrProtocol.h
                   src/probe/ProtocolCrypto.cpp src/probe/ProtocolCrypto.h)
    target_link_libraries(zalo-qr-probe PRIVATE Qt6::Widgets Qt6::Network OpenSSL::Crypto)
endif()

# ---------------------------------------------------------------------------
# Native shell
# ---------------------------------------------------------------------------

set(APP_QML_FILES
    qml/Main.qml
    qml/Theme.qml
    qml/Sidebar.qml
    qml/ConversationDelegate.qml
    qml/ChatPane.qml
    qml/MessageDelegate.qml
    qml/NativeShell.qml
    qml/Avatar.qml
    qml/Icon.qml
    qml/IconButton.qml
    qml/DiagnosticsDialog.qml
    qml/RuntimeShell.qml
    qml/WebUnavailable.qml
)

if(ZALO_HAVE_WEBENGINE)
    list(APPEND APP_QML_FILES qml/RendererHost.qml qml/WebShell.qml)
endif()

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_executable(zalonative
    src/app/main.cpp
    src/app/AppController.cpp
    src/app/AppController.h
    src/app/QrImageProvider.cpp
    src/app/QrImageProvider.h
)

qt_add_qml_module(zalonative
    URI ZaloNative
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES ${APP_QML_FILES}
    RESOURCES
        renderer/stub.html
        renderer/stub.js
        renderer/demo-state.json
        qml/icons/app.svg
)

target_link_libraries(zalonative PRIVATE
    zalonative_core
    zalonative_backend
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
)

target_compile_definitions(zalonative PRIVATE ZALO_VERSION="${PROJECT_VERSION}")

if(ZALO_HAVE_WEBENGINE)
    target_link_libraries(zalonative PRIVATE Qt6::WebEngineQuick Qt6::WebChannel)
    target_compile_definitions(zalonative PRIVATE ZALO_HAVE_WEBENGINE=1)
endif()

# ---------------------------------------------------------------------------
# Bootstrap probe (diagnostic only; injects no host APIs and never logs in)
# ---------------------------------------------------------------------------

if(ZALO_HAVE_WEBENGINE)
    add_executable(zalo-bootstrap-probe tools/bootstrap-probe/main.cpp)
    target_link_libraries(zalo-bootstrap-probe PRIVATE
        zalonative_core
        Qt6::Gui
        Qt6::WebEngineQuick
        Qt6::WebChannel
    )
    target_compile_definitions(zalo-bootstrap-probe PRIVATE
        ZALO_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
endif()

install(TARGETS zalonative RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/io.github.quytttb.ZaloNative.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES LICENSE NOTICE.md
        DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/zalo-native-linux)
install(FILES third_party/ctdozlite/LICENSE-MIT
        DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/zalo-native-linux/third_party/ctdozlite)
install(FILES qml/icons/app.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME io.github.quytttb.ZaloNative.svg)

if(BUILD_TESTING AND ZALO_BUILD_TESTS)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

include(packaging/CPackConfig.cmake)
