# Focused offscreen checks also work with the production Qt kit, which omits QtTest.
if(SNOW_SHOT_BUILD_UPDATE_TESTS AND NOT SNOW_SHOT_BUILD_TESTS)
    add_executable(snow-shot-update-settings-tests tests/settings_catalog_tests.cpp)
    target_link_libraries(snow-shot-update-settings-tests PRIVATE snow_shot_settings_search Qt6::Core)
    add_test(NAME snow-shot-update-settings-tests COMMAND snow-shot-update-settings-tests)
    set_tests_properties(snow-shot-update-settings-tests PROPERTIES LABELS "unit;windows" TIMEOUT 60)
endif()
