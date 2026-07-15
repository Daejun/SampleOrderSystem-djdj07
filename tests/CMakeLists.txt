include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(sample_order_tests
    PlaceholderTest.cpp
    JsonStoreTest.cpp
    SampleRepositoryTest.cpp
    SampleControllerTest.cpp
    ConsoleViewTest.cpp
    OrderInputTest.cpp
    OrderRepositoryTest.cpp
    OrderControllerTest.cpp
    ProductionQueueTest.cpp
    MonitoringServiceTest.cpp
    MonitoringControllerTest.cpp
    EndToEndTest.cpp
    dummygen/DummyGenerator.cpp
    dummygen/SchemaValidator.cpp
)
target_include_directories(sample_order_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sample_order_tests PRIVATE sample_order_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(sample_order_tests)
