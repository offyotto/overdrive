cmake_minimum_required(VERSION 3.20)
project(OverdriveNativeTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(OVERDRIVE_CORE
    ../Source/Overdrive/Core/OverdriveSimulation.cpp
    ../Source/Overdrive/Core/OverdriveHandling.cpp
    ../Source/Overdrive/Core/OverdriveCamera.cpp
)
add_executable(OverdriveSimulationTests SimulationTests.cpp ${OVERDRIVE_CORE})
add_executable(OverdriveMovementTests MovementTests.cpp ${OVERDRIVE_CORE})
foreach(target OverdriveSimulationTests OverdriveMovementTests)
    target_include_directories(${target} PRIVATE ../Source/Overdrive)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -pedantic)
    endif()
endforeach()
enable_testing()
add_test(NAME NativeRaceAndController COMMAND OverdriveSimulationTests)
add_test(NAME SkillfulMovementAndCamera COMMAND OverdriveMovementTests)
