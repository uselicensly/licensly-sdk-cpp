cmake_minimum_required(VERSION 3.20)

# ── Golden fixture verify (no network required) ───────────────────────────────
add_executable(test_golden_verify test_golden_verify.cpp)

target_include_directories(test_golden_verify
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${SODIUM_INCLUDE_DIRS}
)

target_link_libraries(test_golden_verify
    PRIVATE licensly ${SODIUM_LIBRARIES}
)

target_compile_definitions(test_golden_verify
    PRIVATE FIXTURES_DIR="${CMAKE_SOURCE_DIR}/.contract/fixtures"
)

add_test(NAME golden_verify COMMAND test_golden_verify)

# ── Integration test (requires LICENSLY_URL env) ─────────────────────────────
add_executable(test_integration test_integration.cpp)

target_include_directories(test_integration
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${SODIUM_INCLUDE_DIRS}
        ${CURL_INCLUDE_DIRS}
)

target_link_libraries(test_integration
    PRIVATE licensly ${SODIUM_LIBRARIES} CURL::libcurl
)

add_test(NAME integration COMMAND test_integration)
set_tests_properties(integration PROPERTIES
    ENVIRONMENT "LICENSLY_SKIP_INTEGRATION=1"
)
