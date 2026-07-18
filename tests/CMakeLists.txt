add_executable(
  pyrelune_kernel_test
  "${CMAKE_CURRENT_SOURCE_DIR}/kernel_test.cpp"
)

target_link_libraries(
  pyrelune_kernel_test
  PRIVATE
    pyrelune_core
)

add_test(
  NAME pyrelune.kernel
  COMMAND pyrelune_kernel_test
)

add_executable(
  pyrelune_protocol_test
  "${CMAKE_CURRENT_SOURCE_DIR}/protocol_test.cpp"
)

target_link_libraries(
  pyrelune_protocol_test
  PRIVATE
    pyrelune_core
)

add_test(
  NAME pyrelune.protocol
  COMMAND pyrelune_protocol_test
)
