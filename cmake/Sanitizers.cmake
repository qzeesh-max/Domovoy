# Sanitizers.cmake
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_MSAN "Enable MemorySanitizer" OFF)

set(SANITIZER_FLAGS "")

if(ENABLE_ASAN)
    list(APPEND SANITIZER_FLAGS "-fsanitize=address")
endif()

if(ENABLE_TSAN)
    list(APPEND SANITIZER_FLAGS "-fsanitize=thread")
endif()

if(ENABLE_UBSAN)
    list(APPEND SANITIZER_FLAGS "-fsanitize=undefined")
endif()

if(ENABLE_MSAN)
    list(APPEND SANITIZER_FLAGS "-fsanitize=memory")
endif()

if(SANITIZER_FLAGS)
    # Check if compiler supports it
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        add_compile_options(${SANITIZER_FLAGS})
        add_link_options(${SANITIZER_FLAGS})
    elseif(MSVC)
        # MSVC uses different flags for ASAN
        if(ENABLE_ASAN)
            add_compile_options(/fsanitize=address)
            # Link options are usually automatically handled by MSVC if we pass it to compile
        endif()
    endif()
endif()
