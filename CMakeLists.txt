cmake_minimum_required(VERSION 3.24)
project(med_auras LANGUAGES CXX)

add_executable(med-auras WIN32
    src/main.cpp
    src/medication.cpp
    src/storage.cpp
)
target_compile_features(med-auras PRIVATE cxx_std_20)
target_compile_definitions(med-auras PRIVATE UNICODE _UNICODE)

if(MSVC)
    target_compile_options(med-auras PRIVATE /W4 /permissive-)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(medication-test
        tests/medication_test.cpp
        src/medication.cpp
        src/storage.cpp
    )
    target_compile_features(medication-test PRIVATE cxx_std_20)
    target_include_directories(medication-test PRIVATE src)

    if(MSVC)
        target_compile_options(medication-test PRIVATE /W4 /permissive-)
    endif()

    add_test(NAME medication-model COMMAND medication-test)
endif()
