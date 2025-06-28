# Coverage.cmake - Add coverage support to the project

option(ENABLE_COVERAGE "Enable code coverage" ON)

if(ENABLE_COVERAGE)
    message(STATUS "Enabling code coverage support")
    
    # Print compiler information if available
    if(CMAKE_CXX_COMPILER_ID)
        message(STATUS "Using C++ compiler: ${CMAKE_CXX_COMPILER} (${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION})")
    else()
        message(STATUS "Using C++ compiler: ${CMAKE_CXX_COMPILER} (compiler ID not yet determined)")
    endif()
    
    # Determine compiler type - check both ID and executable name as fallback
    set(IS_GCC FALSE)
    set(IS_CLANG FALSE)
    
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set(IS_GCC TRUE)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(IS_CLANG TRUE)
    else()
        # Fallback: check compiler executable name
        get_filename_component(COMPILER_NAME "${CMAKE_CXX_COMPILER}" NAME)
        if(COMPILER_NAME MATCHES ".*g\\+\\+.*")
            set(IS_GCC TRUE)
            message(STATUS "Detected GCC from compiler name: ${COMPILER_NAME}")
        elseif(COMPILER_NAME MATCHES ".*clang\\+\\+.*")
            set(IS_CLANG TRUE)
            message(STATUS "Detected Clang from compiler name: ${COMPILER_NAME}")
        endif()
    endif()
    
    if(IS_GCC)
        # GCC coverage setup
        set(COVERAGE_FLAGS "--coverage")
        
        # Apply to all targets
        add_compile_options(${COVERAGE_FLAGS})
        add_link_options(${COVERAGE_FLAGS})
        
        message(STATUS "Code coverage enabled with GCC/gcov")
        
        # Find required tools for GCC
        find_program(LCOV_PATH lcov)
        find_program(GENHTML_PATH genhtml)
        
        if(LCOV_PATH AND GENHTML_PATH)
            # Add custom target for coverage report generation (GCC)
            add_custom_target(coverage
                COMMAND ${CMAKE_COMMAND} -E remove_directory coverage_html
                COMMAND ${CMAKE_COMMAND} -E make_directory coverage_html
                COMMAND ${LCOV_PATH} --directory . --zerocounters
                COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
                COMMAND ${LCOV_PATH} --directory . --capture --output-file coverage.info
                COMMAND ${LCOV_PATH} --remove coverage.info '/usr/*' '*/tests/*' '*/build/*' '*/vcpkg/*' --output-file coverage_filtered.info
                COMMAND ${GENHTML_PATH} coverage_filtered.info --output-directory coverage_html
                COMMAND ${CMAKE_COMMAND} -E echo "Coverage report generated in coverage_html/index.html"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Generating code coverage report with lcov"
            )
            
            # Add a target to clean coverage data
            add_custom_target(coverage-clean
                COMMAND ${LCOV_PATH} --directory . --zerocounters
                COMMAND ${CMAKE_COMMAND} -E remove coverage.info coverage_filtered.info
                COMMAND ${CMAKE_COMMAND} -E remove_directory coverage_html
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Cleaning coverage data"
            )
            
            # Add comprehensive target for complete coverage analysis
            add_custom_target(run-coverage
                COMMAND ${CMAKE_COMMAND} -E echo "=== Starting GCC Coverage Analysis ==="
                COMMAND ${CMAKE_COMMAND} -E echo "1. Cleaning previous coverage data..."
                COMMAND ${LCOV_PATH} --directory . --zerocounters
                COMMAND ${CMAKE_COMMAND} -E remove_directory coverage_html
                COMMAND ${CMAKE_COMMAND} -E make_directory coverage_html
                COMMAND ${CMAKE_COMMAND} -E echo "2. Building tests..."
                COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${CMAKE_PROJECT_NAME}_test
                COMMAND ${CMAKE_COMMAND} -E echo "3. Running tests with coverage instrumentation..."
                COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --verbose
                COMMAND ${CMAKE_COMMAND} -E echo "4. Capturing coverage data..."
                COMMAND ${LCOV_PATH} --directory . --capture --output-file coverage.info
                COMMAND ${CMAKE_COMMAND} -E echo "5. Filtering coverage data..."
                COMMAND ${LCOV_PATH} --remove coverage.info '/usr/*' '*/tests/*' '*/build/*' '*/vcpkg/*' --output-file coverage_filtered.info
                COMMAND ${CMAKE_COMMAND} -E echo "6. Generating HTML coverage report..."
                COMMAND ${GENHTML_PATH} coverage_filtered.info --output-directory coverage_html --demangle-cpp --sort --title "Gladius Coverage Report" --function-coverage --branch-coverage
                COMMAND ${CMAKE_COMMAND} -E echo "7. Generating summary..."
                COMMAND ${LCOV_PATH} --summary coverage_filtered.info
                COMMAND ${CMAKE_COMMAND} -E echo "=== Coverage Analysis Complete ==="
                COMMAND ${CMAKE_COMMAND} -E echo "📊 HTML Report: file://${CMAKE_BINARY_DIR}/coverage_html/index.html"
                COMMAND ${CMAKE_COMMAND} -E echo "📄 LCOV Data: ${CMAKE_BINARY_DIR}/coverage_filtered.info"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Running complete GCC coverage analysis workflow"
                DEPENDS ${CMAKE_PROJECT_NAME}_test
            )
        else()
            message(WARNING "lcov or genhtml not found. Coverage target will not be available.")
        endif()
        
    elseif(IS_CLANG)
        # Clang coverage setup using source-based coverage
        set(COVERAGE_COMPILE_FLAGS "-fprofile-instr-generate" "-fcoverage-mapping")
        set(COVERAGE_LINK_FLAGS "-fprofile-instr-generate")
        
        # Check if the profile runtime library exists
        execute_process(
            COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libclang_rt.profile-x86_64.a
            OUTPUT_VARIABLE PROFILE_LIB_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        
        # If the library doesn't exist in the expected location, try to find it
        if(NOT EXISTS "${PROFILE_LIB_PATH}" OR PROFILE_LIB_PATH STREQUAL "libclang_rt.profile-x86_64.a")
            message(STATUS "Clang profile runtime not found at default location, searching...")
            
            # Try to find the library in common locations
            find_file(CLANG_RT_PROFILE_LIB
                NAMES libclang_rt.profile-x86_64.a
                PATHS
                    /usr/lib/llvm-*/lib/clang/*/lib/linux
                    /usr/lib/clang/*/lib/linux
                    /usr/local/lib/clang/*/lib/linux
                    /opt/llvm/lib/clang/*/lib/linux
                NO_DEFAULT_PATH
            )
            
            if(CLANG_RT_PROFILE_LIB)
                message(STATUS "Found Clang profile runtime at: ${CLANG_RT_PROFILE_LIB}")
                get_filename_component(RT_LIB_DIR "${CLANG_RT_PROFILE_LIB}" DIRECTORY)
                set(COVERAGE_LINK_FLAGS "-fprofile-instr-generate" "-L${RT_LIB_DIR}")
            else()
                message(WARNING "Clang profile runtime library not found. You may need to install it:")
                message(WARNING "  sudo apt-get install libc++-dev libc++abi-dev")
                message(WARNING "  or")
                message(WARNING "  sudo apt-get install clang-14 # (or your clang version)")
                message(WARNING "Falling back to GCC-style coverage flags")
                set(COVERAGE_COMPILE_FLAGS "--coverage")
                set(COVERAGE_LINK_FLAGS "--coverage")
            endif()
        else()
            message(STATUS "Found Clang profile runtime at: ${PROFILE_LIB_PATH}")
        endif()
        
        # Apply to all targets
        add_compile_options(${COVERAGE_COMPILE_FLAGS})
        add_link_options(${COVERAGE_LINK_FLAGS})
        
        message(STATUS "Code coverage enabled with Clang/llvm-cov")
        message(STATUS "Coverage compile flags: ${COVERAGE_COMPILE_FLAGS}")
        message(STATUS "Coverage link flags: ${COVERAGE_LINK_FLAGS}")
        
        # Find required tools for Clang
        find_program(LLVM_PROFDATA_PATH llvm-profdata)
        find_program(LLVM_COV_PATH llvm-cov)
        
        if(LLVM_PROFDATA_PATH AND LLVM_COV_PATH)
            # Set environment variable for profile data
            set(LLVM_PROFILE_FILE "${CMAKE_BINARY_DIR}/coverage_%p.profraw")
            
            # Add custom target for coverage report generation (Clang)
            add_custom_target(coverage
                COMMAND ${CMAKE_COMMAND} -E remove_directory coverage_html
                COMMAND ${CMAKE_COMMAND} -E make_directory coverage_html
                COMMAND ${CMAKE_COMMAND} -E env LLVM_PROFILE_FILE=${LLVM_PROFILE_FILE} ${CMAKE_CTEST_COMMAND} --output-on-failure
                COMMAND ${LLVM_PROFDATA_PATH} merge -sparse ${CMAKE_BINARY_DIR}/coverage_*.profraw -o ${CMAKE_BINARY_DIR}/coverage.profdata
                COMMAND ${LLVM_COV_PATH} show $<TARGET_FILE:${CMAKE_PROJECT_NAME}_test> -instr-profile=${CMAKE_BINARY_DIR}/coverage.profdata -format=html -output-dir=coverage_html -ignore-filename-regex='/usr/.*|.*/tests/.*|.*/build/.*|.*/vcpkg/.*'
                COMMAND ${LLVM_COV_PATH} report $<TARGET_FILE:${CMAKE_PROJECT_NAME}_test> -instr-profile=${CMAKE_BINARY_DIR}/coverage.profdata -ignore-filename-regex='/usr/.*|.*/tests/.*|.*/build/.*|.*/vcpkg/.*'
                COMMAND ${CMAKE_COMMAND} -E echo "Coverage report generated in coverage_html/index.html"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Generating code coverage report with llvm-cov"
                DEPENDS ${CMAKE_PROJECT_NAME}_test
            )
            
            # Add target for text-based coverage report
            add_custom_target(coverage-report
                COMMAND ${CMAKE_COMMAND} -E env LLVM_PROFILE_FILE=${LLVM_PROFILE_FILE} ${CMAKE_CTEST_COMMAND} --output-on-failure
                COMMAND ${LLVM_PROFDATA_PATH} merge -sparse ${CMAKE_BINARY_DIR}/coverage_*.profraw -o ${CMAKE_BINARY_DIR}/coverage.profdata
                COMMAND ${LLVM_COV_PATH} report $<TARGET_FILE:${CMAKE_PROJECT_NAME}_test> -instr-profile=${CMAKE_BINARY_DIR}/coverage.profdata -ignore-filename-regex='/usr/.*|.*/tests/.*|.*/build/.*|.*/vcpkg/.*'
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Generating text coverage report with llvm-cov"
                DEPENDS ${CMAKE_PROJECT_NAME}_test
            )
            
            # Add target for exporting coverage in lcov format (for CI/CD compatibility)
            add_custom_target(coverage-lcov
                COMMAND ${CMAKE_COMMAND} -E env LLVM_PROFILE_FILE=${LLVM_PROFILE_FILE} ${CMAKE_CTEST_COMMAND} --output-on-failure
                COMMAND ${LLVM_PROFDATA_PATH} merge -sparse ${CMAKE_BINARY_DIR}/coverage_*.profraw -o ${CMAKE_BINARY_DIR}/coverage.profdata
                COMMAND ${LLVM_COV_PATH} export $<TARGET_FILE:${CMAKE_PROJECT_NAME}_test> -instr-profile=${CMAKE_BINARY_DIR}/coverage.profdata -format=lcov -ignore-filename-regex='/usr/.*|.*/tests/.*|.*/build/.*|.*/vcpkg/.*' > coverage.lcov
                COMMAND ${CMAKE_COMMAND} -E echo "Coverage exported to coverage.lcov"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Exporting coverage in lcov format"
                DEPENDS ${CMAKE_PROJECT_NAME}_test
            )
            
            # Add a target to clean coverage data
            add_custom_target(coverage-clean
                COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/coverage_*.profraw
                COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/coverage.profdata
                COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/coverage.lcov
                COMMAND ${CMAKE_COMMAND} -E remove_directory coverage_html
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Cleaning coverage data"
            )
            
            # Add comprehensive target for complete coverage analysis
            add_custom_target(run-coverage
                COMMAND ${CMAKE_COMMAND} -E echo "=== Starting Clang Coverage Analysis ==="
                COMMAND ${CMAKE_COMMAND} -E echo "1. Cleaning previous coverage data..."
                COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/coverage_*.profraw
                COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/coverage.profdata
                COMMAND ${CMAKE_COMMAND} -E remove_directory coverage_html
                COMMAND ${CMAKE_COMMAND} -E make_directory coverage_html
                COMMAND ${CMAKE_COMMAND} -E echo "2. Building tests..."
                COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${CMAKE_PROJECT_NAME}_test
                COMMAND ${CMAKE_COMMAND} -E echo "3. Running tests with coverage instrumentation..."
                COMMAND ${CMAKE_COMMAND} -E env LLVM_PROFILE_FILE=${LLVM_PROFILE_FILE} ${CMAKE_CTEST_COMMAND} --output-on-failure --verbose
                COMMAND ${CMAKE_COMMAND} -E echo "4. Processing coverage data..."
                COMMAND ${LLVM_PROFDATA_PATH} merge -sparse ${CMAKE_BINARY_DIR}/coverage_*.profraw -o ${CMAKE_BINARY_DIR}/coverage.profdata
                COMMAND ${CMAKE_COMMAND} -E echo "5. Generating HTML coverage report..."
                COMMAND ${LLVM_COV_PATH} show $<TARGET_FILE:${CMAKE_PROJECT_NAME}_test> -instr-profile=${CMAKE_BINARY_DIR}/coverage.profdata -format=html -output-dir=coverage_html -ignore-filename-regex='/usr/.*|.*/tests/.*|.*/build/.*|.*/vcpkg/.*' -show-line-counts-or-regions -show-expansions
                COMMAND ${CMAKE_COMMAND} -E echo "6. Generating text summary..."
                COMMAND ${LLVM_COV_PATH} report $<TARGET_FILE:${CMAKE_PROJECT_NAME}_test> -instr-profile=${CMAKE_BINARY_DIR}/coverage.profdata -ignore-filename-regex='/usr/.*|.*/tests/.*|.*/build/.*|.*/vcpkg/.*'
                COMMAND ${CMAKE_COMMAND} -E echo "7. Exporting lcov format for CI/CD..."
                COMMAND ${LLVM_COV_PATH} export $<TARGET_FILE:${CMAKE_PROJECT_NAME}_test> -instr-profile=${CMAKE_BINARY_DIR}/coverage.profdata -format=lcov -ignore-filename-regex='/usr/.*|.*/tests/.*|.*/build/.*|.*/vcpkg/.*' > coverage.lcov
                COMMAND ${CMAKE_COMMAND} -E echo "=== Coverage Analysis Complete ==="
                COMMAND ${CMAKE_COMMAND} -E echo "📊 HTML Report: file://${CMAKE_BINARY_DIR}/coverage_html/index.html"
                COMMAND ${CMAKE_COMMAND} -E echo "📄 LCOV Export: ${CMAKE_BINARY_DIR}/coverage.lcov"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Running complete Clang coverage analysis workflow"
                DEPENDS ${CMAKE_PROJECT_NAME}_test
            )
        else()
            message(WARNING "llvm-profdata or llvm-cov not found. Coverage target will not be available.")
        endif()
    else()
        message(WARNING "Coverage is only supported with GCC or Clang compilers")
        message(STATUS "Current compiler: ${CMAKE_CXX_COMPILER}")
        message(STATUS "Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
        get_filename_component(COMPILER_NAME "${CMAKE_CXX_COMPILER}" NAME)
        message(STATUS "Compiler name: ${COMPILER_NAME}")
    endif()
endif()
