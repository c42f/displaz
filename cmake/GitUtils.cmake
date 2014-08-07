find_package(Git)

# Call git describe with given argument list and return the output, stripped of
# whitespace
#
# If git returns error, set outputVar to git_describe-NOTFOUND
function(git_describe outputVar)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --show-toplevel
                    OUTPUT_VARIABLE gitTopLevelDir
                    RESULT_VARIABLE gitResult
                    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (NOT gitResult STREQUAL "0")
        set(${outputVar} "git_describe-NOTFOUND" PARENT_SCOPE)
        return()
    endif()
    # Crude way to force cmake rerun for any changes a developer makes to the
    # HEAD revision - we force cmake to add a file dependency on the internal
    # git log file for HEAD.  Note that this can't detect changes to git
    # describe output due to added tags, but it detects most other things.
    configure_file("${gitTopLevelDir}/.git/logs/HEAD" git_HEAD_change_check COPYONLY)
    # Call git describe
    execute_process(COMMAND ${GIT_EXECUTABLE} describe ${ARGN}
                    OUTPUT_VARIABLE gitOutput
                    RESULT_VARIABLE gitResult
                    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (NOT gitResult STREQUAL "0")
        set(${outputVar} "git_describe-NOTFOUND" PARENT_SCOPE)
    else()
        set(${outputVar} ${gitOutput} PARENT_SCOPE)
    endif()
endfunction()

