add_library(zalonative_backend STATIC
    NativeQrLogin.cpp
    NativeQrLogin.h
    PcQrProtocol.cpp
    PcQrProtocol.h
)

target_include_directories(zalonative_backend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(zalonative_backend PUBLIC Qt6::Core Qt6::Gui Qt6::Network)
set_target_properties(zalonative_backend PROPERTIES POSITION_INDEPENDENT_CODE ON)
