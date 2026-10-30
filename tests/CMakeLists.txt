set(CMAKE_AUTOMOC ON)

function(add_zalo_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE zalonative_core Qt6::Test)
    target_compile_definitions(${name} PRIVATE
        ZALO_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
        ZALO_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_zalo_test(test_asar)
add_zalo_test(test_bridge)
add_zalo_test(test_host_api)
add_zalo_test(test_models)
add_zalo_test(test_runtime)
add_zalo_test(test_scheme)
add_zalo_test(test_preload)
add_zalo_test(test_importer)

# M1 sync spike: only meaningful in a build that actually has WebEngine.
if(ZALO_HAVE_WEBENGINE)
    add_executable(test_sync_scheme test_sync_scheme.cpp)
    target_link_libraries(test_sync_scheme PRIVATE
        zalonative_core Qt6::Test Qt6::WebEngineQuick Qt6::WebChannel)
    add_test(NAME test_sync_scheme COMMAND test_sync_scheme)
    set_tests_properties(test_sync_scheme PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;QTWEBENGINE_DISABLE_SANDBOX=1"
        TIMEOUT 60)

    add_executable(test_csp_scheme test_csp_scheme.cpp)
    target_link_libraries(test_csp_scheme PRIVATE
        zalonative_core Qt6::Test Qt6::WebEngineQuick)
    target_compile_definitions(test_csp_scheme PRIVATE
        ZALO_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME test_csp_scheme COMMAND test_csp_scheme)
    set_tests_properties(test_csp_scheme PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;QTWEBENGINE_DISABLE_SANDBOX=1"
        TIMEOUT 60)

    add_executable(test_host_preload test_host_preload.cpp)
    target_link_libraries(test_host_preload PRIVATE
        zalonative_core Qt6::Test Qt6::WebEngineQuick)
    target_compile_definitions(test_host_preload PRIVATE
        ZALO_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME test_host_preload COMMAND test_host_preload)
    set_tests_properties(test_host_preload PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;QTWEBENGINE_DISABLE_SANDBOX=1"
        TIMEOUT 60)
endif()

if(TARGET zalo-qr-probe)
    add_executable(test_qr_protocol test_qr_protocol.cpp ../src/probe/QrProtocol.cpp ../src/probe/ProtocolCrypto.cpp)
    target_include_directories(test_qr_protocol PRIVATE ../src/probe)
    target_link_libraries(test_qr_protocol PRIVATE Qt6::Core Qt6::Test OpenSSL::Crypto)
    add_test(NAME test_qr_protocol COMMAND test_qr_protocol)
endif()

add_test(NAME cli_inspect
         COMMAND zalort inspect "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/sample.asar")
set_tests_properties(cli_inspect PROPERTIES
                     PASS_REGULAR_EXPRESSION "sample-zalo-runtime")

add_test(NAME cli_inspect_app
         COMMAND zalort inspect "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/app-runtime")
set_tests_properties(cli_inspect_app PROPERTIES
                     PASS_REGULAR_EXPRESSION "9.9.9-fixture")

add_test(NAME cli_list
         COMMAND zalort list "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/sample.asar" node_modules)
set_tests_properties(cli_list PROPERTIES
                     PASS_REGULAR_EXPRESSION "node_modules/fake-native")

add_test(NAME cli_bridge_spec COMMAND zalort bridge-spec)
set_tests_properties(cli_bridge_spec PROPERTIES
                     PASS_REGULAR_EXPRESSION "zalonative.bridge/1")

if(TARGET zalonative)
    add_test(NAME app_self_test COMMAND zalonative --self-test)
    add_test(NAME app_smoke_test COMMAND zalonative --smoke-test --demo)
    add_test(NAME app_native_smoke_test COMMAND zalonative --smoke-test)
    add_test(NAME app_web_smoke_test COMMAND zalonative --smoke-test --web)
    set_tests_properties(app_self_test app_smoke_test app_native_smoke_test app_web_smoke_test
        PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LOGGING_RULES=qt.qml.binding.removal.info=false"
        TIMEOUT 30)
    set_tests_properties(app_smoke_test app_native_smoke_test app_web_smoke_test PROPERTIES
        FAIL_REGULAR_EXPRESSION "ReferenceError;TypeError;Unable to assign;Cannot assign;Cannot anchor;failed to load component;is not a type;module .* is not installed")

    # Negative control: the strict smoke-test handler must turn a QML warning
    # into a non-zero exit, independent of the regex above.
    add_test(NAME app_qml_warning_negative COMMAND zalonative --smoke-test --demo)
    set_tests_properties(app_qml_warning_negative PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;ZALO_TEST_QML_WARNING=1"
        TIMEOUT 30
        WILL_FAIL TRUE)
endif()
