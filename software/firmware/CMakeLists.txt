#######################################################################
# Initial setup parameters
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR arm)
cmake_minimum_required(VERSION 3.20)

# Package includes
find_package(Git REQUIRED)
find_package(Perl REQUIRED)

# Build configuration options
option(USE_SEGGER_RTT "Enable logging via SEGGER RTT" OFF)

# Specify cross-compilers and tools
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER  arm-none-eabi-gcc)
set(CMAKE_AR arm-none-eabi-ar)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_OBJDUMP arm-none-eabi-objdump)
set(CMAKE_SIZE arm-none-eabi-size)

# Skip link step during compiler validation
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Specify compiler variables
set (RUNTIME_LIBRARY "--specs=nano.specs")
set (RUNTIME_LIBRARY_SYSCALLS "--specs=nosys.specs")

# Specify common compile flags and options
set(CXX_FLAGS
    -fno-rtti
    -fno-exceptions
    -fno-threadsafe-statics
)
add_compile_options(
    -fdata-sections
    -ffunction-sections
    ${RUNTIME_LIBRARY}
    "$<$<COMPILE_LANGUAGE:CXX>:${CXX_FLAGS}>"
)

#######################################################################
# Project settings
project(printalyzer)
set(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(EXTERNAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external)

# MCU Information
set(MCU_FAMILY STM32F4xx)
set(MCU_MODEL STM32F411xE)
set(CPU_PARAMETERS
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard)

set(STARTUP_SCRIPT ${PROJECT_DIR}/system/startup_stm32f411retx.s)
set(MCU_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32F411RETX_FLASH.ld)

add_compile_options(
    ${CPU_PARAMETERS}
    -fstack-protector-strong
    "$<$<CONFIG:Debug>:-Og;-g3;-ggdb>"
    "$<$<CONFIG:Release>:-Og;-g0>"
)

add_compile_definitions(
    ${MCU_MODEL}
)

#######################################################################
# Build information variables
execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --always --dirty --exclude '*'
        OUTPUT_VARIABLE BUILD_DESCRIBE
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(TIMESTAMP BUILD_DATE "%Y-%m-%d %H:%M")

#######################################################################

set(EXECUTABLE ${CMAKE_PROJECT_NAME})
enable_language(C ASM)

# Set linker output suffixes
set (CMAKE_EXECUTABLE_SUFFIX ".elf")
set (CMAKE_STATIC_LIBRARY_SUFFIX ".a")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# Headers
set(EXTERNAL_INCLUDE_DIRECTORIES
    ${EXTERNAL_DIR}/drivers/stm32f4xx/include
    ${EXTERNAL_DIR}/drivers/stm32f4xx/include/Legacy
    ${EXTERNAL_DIR}/drivers/cmsis/include/
    ${EXTERNAL_DIR}/usb/core/include
    ${EXTERNAL_DIR}/usb/class/include
    ${EXTERNAL_DIR}/usb/target
    ${EXTERNAL_DIR}/fatfs/src
    ${EXTERNAL_DIR}/fatfs/target
    ${EXTERNAL_DIR}/freertos/include
    ${EXTERNAL_DIR}/freertos/CMSIS_RTOS_V2
    ${EXTERNAL_DIR}/freertos/portable/GCC/ARM_CM4F
    ${EXTERNAL_DIR}/easylogger/include
    ${EXTERNAL_DIR}/u8g2/csrc
    ${EXTERNAL_DIR}/cmsis-dsp/include)
set(PROJECT_INCLUDE_DIRECTORIES
    ${PROJECT_DIR}
    ${PROJECT_DIR}/system
    ${PROJECT_DIR}/drivers)

# Sources
file(GLOB_RECURSE EXTERNAL_SOURCES
    ${EXTERNAL_DIR}/drivers/*.c
    ${EXTERNAL_DIR}/easylogger/src/*.c
    ${EXTERNAL_DIR}/fatfs/*.c
    ${EXTERNAL_DIR}/freertos/*.c
    ${EXTERNAL_DIR}/u8g2/csrc/*.c
    ${EXTERNAL_DIR}/usb/*.c)
file(GLOB_RECURSE PROJECT_SOURCES FOLLOW_SYMLINKS
    ${PROJECT_DIR}/*.c)

# Executable files
add_executable(${EXECUTABLE}
    ${EXTERNAL_SOURCES} 
    ${PROJECT_SOURCES}
    ${STARTUP_SCRIPT})

if(USE_SEGGER_RTT)
    add_subdirectory(${EXTERNAL_DIR}/RTT)
    target_compile_definitions(${EXECUTABLE} PRIVATE USE_SEGGER_RTT)
    target_link_libraries(${EXECUTABLE} PRIVATE RTT)
endif()

# Embedded macros(defines)
target_compile_definitions(${EXECUTABLE} PRIVATE
    USE_HAL_DRIVER
    $<$<CONFIG:DEBUG>:DEBUG>
    APP_BUILD_DATE="${BUILD_DATE}"
    APP_BUILD_DESCRIBE="${BUILD_DESCRIBE}")

# Add header directories (AFTER add_executable)
target_include_directories(${EXECUTABLE} PRIVATE
    ${EXTERNAL_INCLUDE_DIRECTORIES}
    ${PROJECT_INCLUDE_DIRECTORIES})

# Compiler and linker options
target_compile_options(${EXECUTABLE} PRIVATE
    -Wall
    -Wformat-overflow
    -Wmaybe-uninitialized
    -Wno-unused-parameter
    $<$<COMPILE_LANGUAGE:CXX>:
        -Wno-volatile
        -Wold-style-cast
        -Wuseless-cast
        -Wsuggest-override>
)

target_link_libraries(${EXECUTABLE} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/external/cmsis-dsp/lib/libarm_cortexM4lf_math.a)

target_link_options(${EXECUTABLE} PRIVATE
    -T${MCU_LINKER_SCRIPT}
    ${CPU_PARAMETERS}
    -Wl,-Map=${CMAKE_PROJECT_NAME}.map
    -Wl,--gc-sections
    ${RUNTIME_LIBRARY_SYSCALLS}
    -u _printf_float
    -Wl,--start-group
    -lc
    -lm
    -Wl,--end-group
    -Wl,--print-memory-usage)

add_custom_command(TARGET ${EXECUTABLE} POST_BUILD
    COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/codesize.pl -s ${CMAKE_SIZE} $<TARGET_FILE:${EXECUTABLE}>)

# Post-build commands to embed the checksum
add_custom_command(TARGET ${EXECUTABLE} POST_BUILD
    DEPENDS ${EXECUTABLE}
    COMMAND ${CMAKE_OBJCOPY} --gap-fill 0xff $<TARGET_FILE:${EXECUTABLE}> ${EXECUTABLE}-temp.elf
    COMMAND ${CMAKE_OBJCOPY} -O binary ${EXECUTABLE}-temp.elf ${EXECUTABLE}-temp.bin
    COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/checksum.pl ${EXECUTABLE}-temp.bin ${EXECUTABLE}-desc.dat
    COMMAND ${CMAKE_OBJCOPY} --update-section .app_descriptor=${EXECUTABLE}-desc.dat --gap-fill 0xff ${EXECUTABLE}.elf ${EXECUTABLE}-out.elf
    COMMAND ${CMAKE_OBJCOPY} -O binary ${EXECUTABLE}-out.elf ${EXECUTABLE}-out.bin
    BYPRODUCTS ${EXECUTABLE}-temp.elf ${EXECUTABLE}-temp.bin ${EXECUTABLE}-desc.dat
)
