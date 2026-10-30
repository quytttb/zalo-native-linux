add_library(zalonative_core STATIC
    AsarArchive.cpp
    AsarArchive.h
    BridgeRouter.cpp
    BridgeRouter.h
    ConversationFilterModel.cpp
    ConversationFilterModel.h
    ConversationModel.cpp
    ConversationModel.h
    LocalScheme.cpp
    LocalScheme.h
    MessageModel.cpp
    MessageModel.h
    RuntimeLocator.cpp
    RuntimeLocator.h
    RuntimeManifest.cpp
    RuntimeManifest.h
)

target_include_directories(zalonative_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(zalonative_core PUBLIC Qt6::Core)
set_target_properties(zalonative_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
