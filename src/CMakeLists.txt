add_library(sample_order_persistence
    persistence/JsonDocument.cpp
)
target_include_directories(sample_order_persistence PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sample_order_persistence PUBLIC nlohmann_json::nlohmann_json)
target_compile_features(sample_order_persistence PUBLIC cxx_std_17)

add_executable(sample_order_app
    main.cpp
    view/ConsoleView.cpp
    controller/MainController.cpp
)
target_include_directories(sample_order_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sample_order_app PRIVATE sample_order_persistence)
