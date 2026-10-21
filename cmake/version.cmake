# Reads VERSION (one line, x.y.z) into LJ_VERSION; lj_write_version_h() generates
# version.h with LJ_VERSION_STR and the short git hash. Included before project().
file(READ "${CMAKE_CURRENT_LIST_DIR}/../VERSION" LJ_VERSION)
string(STRIP "${LJ_VERSION}" LJ_VERSION)
if(NOT LJ_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
  message(FATAL_ERROR "VERSION must be x.y.z, got '${LJ_VERSION}'")
endif()

function(lj_write_version_h out_dir)
  set(LJ_GIT_HASH "unknown")
  find_package(Git QUIET)
  if(GIT_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
                    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                    OUTPUT_VARIABLE hash OUTPUT_STRIP_TRAILING_WHITESPACE
                    RESULT_VARIABLE rc ERROR_QUIET)
    if(rc EQUAL 0 AND hash)
      set(LJ_GIT_HASH "${hash}")
    endif()
  endif()
  configure_file("${CMAKE_SOURCE_DIR}/cmake/version.h.in" "${out_dir}/version.h" @ONLY)
endfunction()
