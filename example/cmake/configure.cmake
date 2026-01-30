function(configure TARGET_NAME)
    if(${ARGC} EQUAL 1)
        set(ENABLE_SSL false)
    else()
        set(ENABLE_SSL ${ARGV1})
    endif()

    GroupSources (include/asio3 "/")
    GroupSources (3rd/asio "/")

    aux_source_directory(. SRC_FILES)
    source_group("" FILES ${SRC_FILES})

    target_include_directories(${TARGET_NAME} PRIVATE
        ${ASIO3_ROOT_DIR}/3rd
        ${ASIO3_ROOT_DIR}/include
    )

    set_target_properties(${TARGET_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    set_property(TARGET ${TARGET_NAME} PROPERTY FOLDER "example")

    target_link_libraries(${TARGET_NAME} ${CMAKE_THREAD_LIBS_INIT})
    target_link_libraries(${TARGET_NAME} ${GENERAL_LIBS_FILE})

    if(${ENABLE_SSL})
        if (MSVC)
            set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
        endif()
        target_include_directories(${TARGET_NAME} PRIVATE
            ${ASIO3_ROOT_DIR}/3rd/openssl/include
        )
        target_link_directories(${TARGET_NAME} PRIVATE ${ASIO3_ROOT_DIR}/${ASIO3_OPENSSL_LIBS_DIR})
        target_link_libraries(${TARGET_NAME} ${OPENSSL_LIBS_FILE})
    endif()

    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE
            /bigobj
            /W4
            /JMC
            $<$<NOT:$<CXX_COMPILER_ID:Clang>>:/MP>
            $<$<NOT:$<CXX_COMPILER_ID:Clang>>:/Zc:__cplusplus>
        )

        target_compile_definitions(${TARGET_NAME} PRIVATE
            _SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING
            _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
        )

        target_link_options(${TARGET_NAME} PRIVATE /SAFESEH:NO)

        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
            target_compile_options(${TARGET_NAME} PRIVATE
                $<$<CONFIG:Debug>:/MTd>
                $<$<CONFIG:Release>:/MT>
                -Wrange-loop-analysis
                -Wthread-safety
                $<$<CONFIG:Release>:/Ob2 /Oi /Ot>
            )
        else()
            target_compile_options(${TARGET_NAME} PRIVATE
                $<$<CONFIG:Debug>:/MTd /Zi>
                $<$<CONFIG:Release>:/MT /Zi /GL>
                $<$<CONFIG:Release>:/Ob2 /Oi /Ot>
            )
            target_link_options(${TARGET_NAME} PRIVATE
                $<$<CONFIG:Release>:/LTCG:incremental>
            )
            set_target_properties(${TARGET_NAME} PROPERTIES
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
            )
        endif ()
    else()
        target_compile_options(${TARGET_NAME} PRIVATE
            -Wall -Wextra -Wpedantic -Wno-unused-parameter
            -fcoroutines
        )

        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
            target_compile_options(${TARGET_NAME} PRIVATE
                -Wrange-loop-analysis
                -Wthread-safety
            )
        endif ()

        if(MINGW OR CYGWIN)
            target_compile_options(${TARGET_NAME} PRIVATE
                -O3
                -Wa,-mbig-obj
            )
        endif()
    endif()
endfunction()
