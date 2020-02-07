#  ToyRT (Runtime library, open source software)
#
# Set label_VERSION to the git version

function(git_version label default_version)
    option(${label}_GIT_VERSION "Get the version string from git describe" ON)
    if(${label}_GIT_VERSION)
        find_package(Git)
        if(GIT_FOUND)
            execute_process(COMMAND "${GIT_EXECUTABLE}"
                describe --dirty=-dirty --always --tags
                OUTPUT_VARIABLE _GIT_DESCRIBE ERROR_QUIET)
            if(_GIT_DESCRIBE)
                string(STRIP ${_GIT_DESCRIBE} ${label}_VERSION)
                set(${label}_VERSION ${${label}_VERSION} PARENT_SCOPE)
            endif()
        endif()
    endif()
    if(NOT ${label}_VERSION)
        set(${label}_VERSION ${default_version} PARENT_SCOPE)
    endif()
    message(STATUS "Version string is ${${label}_VERSION}")
endfunction()
