# Validate direct project-target edges during configure; external targets have
# their own dependency graphs and are deliberately outside this rule.
function(drawer_check_dependencies target)
    set(allowed ${ARGN})
    get_target_property(dependencies ${target} LINK_LIBRARIES)
    foreach(dependency IN LISTS dependencies)
        if(dependency MATCHES "^drawer_" AND NOT dependency IN_LIST allowed)
            message(FATAL_ERROR "${target} must not depend on ${dependency}")
        endif()
    endforeach()
endfunction()
