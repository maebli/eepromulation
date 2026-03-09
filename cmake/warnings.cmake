# Shared compiler warning flags
# Included by the root CMakeLists.txt via include(cmake/warnings.cmake).

# Enable compile_commands.json for clangd / clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

function(target_apply_warnings target)
  target_compile_options(${target} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Woverloaded-virtual
    -Wconversion
    -Wsign-conversion
    -Wmisleading-indentation
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
    # Treat warnings as errors in CI (override per-target when needed)
    $<$<BOOL:${WARNINGS_AS_ERRORS}>:-Werror>
  )
endfunction()
