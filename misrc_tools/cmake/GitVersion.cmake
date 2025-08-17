function(get_version_from_git)
    find_package(Git QUIET)
    if(NOT Git_FOUND)
        message(WARNING "Git not found")
        return()
    endif()

    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --dirty --match misrc_tools-*
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE GIT_RESULT
    )

    if(NOT GIT_RESULT EQUAL 0)
        message(WARNING "Failed to get git tag")
        set(VCS_TAG "unknown_version")
        set(VCS_TAG "unknown_version" PARENT_SCOPE)
        return()
    endif()

    string(REGEX REPLACE "misrc_tools-" "" CLEAN_TAG "${GIT_TAG}")
    set(VCS_TAG ${CLEAN_TAG})
    set(VCS_TAG ${CLEAN_TAG} PARENT_SCOPE)
endfunction()

