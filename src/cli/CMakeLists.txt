qt_add_executable(zalort
    main.cpp
)

target_link_libraries(zalort PRIVATE zalonative_core Qt6::Core)
target_compile_definitions(zalort PRIVATE ZALO_VERSION="${PROJECT_VERSION}")

install(TARGETS zalort RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
