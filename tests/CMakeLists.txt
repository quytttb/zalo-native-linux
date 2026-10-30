set(CMAKE_AUTOMOC ON)

function(add_zalo_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE zalonative_core Qt6::Test)
    target_compile_definitions(${name} PRIVATE
        ZALO_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_zalo_test(test_asar)
add_zalo_test(test_bridge)
add_zalo_test(test_models)
add_zalo_test(test_runtime)
add_zalo_test(test_scheme)

add_test(NAME cli_inspect
         COMMAND zalort inspect "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/sample.asar")
set_tests_properties(cli_inspect PROPERTIES
                     PASS_REGULAR_EXPRESSION "sample-zalo-runtime")

add_test(NAME cli_list
         COMMAND zalort list "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/sample.asar" node_modules)
set_tests_properties(cli_list PROPERTIES
                     PASS_REGULAR_EXPRESSION "node_modules/fake-native")

add_test(NAME cli_bridge_spec COMMAND zalort bridge-spec)
set_tests_properties(cli_bridge_spec PROPERTIES
                     PASS_REGULAR_EXPRESSION "zalonative.bridge/1")

if(TARGET zalonative)
    add_test(NAME app_self_test COMMAND zalonative --self-test)
    add_test(NAME app_smoke_test COMMAND zalonative --smoke-test)
    set_tests_properties(app_self_test app_smoke_test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LOGGING_RULES=qt.qml.binding.removal.info=false"
        TIMEOUT 30)
endif()
