llvm_map_components_to_libnames(llvm_libs bitwriter core ipo irreader instcombine instrumentation target linker analysis scalaropts support )

add_executable(svf-ex svf-ex.cpp)
add_executable(analysis analysis.cpp detector.h detector.cpp)

target_link_libraries(svf-ex ${SVF_LIB} ${LLVMCudd} ${llvm_libs})
target_link_libraries(svf-ex ${Z3_DIR}/bin/libz3.a )
set_target_properties( svf-ex PROPERTIES
                       RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )

target_link_libraries(analysis ${SVF_LIB} ${LLVMCudd} ${llvm_libs})
target_link_libraries(analysis ${Z3_DIR}/bin/libz3.a )
set_target_properties(analysis PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )
