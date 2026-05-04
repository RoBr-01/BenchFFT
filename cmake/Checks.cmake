# If there are submodules in this project, make sure they are initialised
function(CHECK_GIT_SUBMODULES)

    find_package(Git QUIET)
    if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git" AND EXISTS "${PROJECT_SOURCE_DIR}/.gitmodules")

        option(GIT_SUBMODULES_CHECK "Check submodules during build" ON)

        if(GIT_SUBMODULES_CHECK)

            message("-- Git submodules found, updating")

            execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                RESULT_VARIABLE GIT_SUBMOD_RESULT
            )

            if(NOT GIT_SUBMOD_RESULT EQUAL "0")
                message(FATAL_ERROR "Git submodule update failed with ${GIT_SUBMOD_RESULT}")
            endif()
        endif()
    else()
        message("-- No git submodules, not updating")
    endif()

endfunction()
