set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

set(rime_console_deps
  ${rime_library}
  ${rime_dict_library}
  ${rime_gears_library}
  ${rime_levers_library}
  ${rime_plugins_library})

set(rime_api_console_src "rime_api_console.cc")
add_executable(rime_api_console ${rime_api_console_src})
target_link_libraries(rime_api_console ${rime_console_deps})

if(EMSCRIPTEN)
    set(rime_emscripten_src "rime_emscripten.cpp" "custom-backend/wasm_fast_indexeddb.cpp")
    add_executable(rime_emscripten ${rime_emscripten_src})
    target_link_libraries(rime_emscripten ${rime_console_deps})
endif()

set(rime_patch_src "rime_patch.cc")
add_executable(rime_patch ${rime_patch_src})
target_link_libraries(rime_patch
  ${rime_library}
  ${rime_levers_library})

# msvc doesn't export all symbols
if(NOT (WIN32 AND BUILD_SHARED_LIBS))

set(rime_console_src "rime_console.cc")
add_executable(rime_console ${rime_console_src})
target_link_libraries(rime_console ${rime_console_deps})

set(rime_dict_manager_src "rime_dict_manager.cc")
add_executable(rime_dict_manager ${rime_dict_manager_src})
target_link_libraries(rime_dict_manager
  ${rime_library}
  ${rime_dict_library}
  ${rime_levers_library})

set(rime_deployer_src "rime_deployer.cc")
add_executable(rime_deployer ${rime_deployer_src})
target_link_libraries(rime_deployer
  ${rime_library}
  ${rime_dict_library}
  ${rime_levers_library})

install(TARGETS rime_deployer DESTINATION ${BIN_INSTALL_DIR})
install(TARGETS rime_dict_manager DESTINATION ${BIN_INSTALL_DIR})

endif()

install(TARGETS rime_patch DESTINATION ${BIN_INSTALL_DIR})

file(COPY ${PROJECT_SOURCE_DIR}/data/minimal/default.yaml
     DESTINATION ${EXECUTABLE_OUTPUT_PATH})
file(COPY ${PROJECT_SOURCE_DIR}/data/minimal/symbols.yaml
     DESTINATION ${EXECUTABLE_OUTPUT_PATH})
file(COPY ${PROJECT_SOURCE_DIR}/data/minimal/essay.txt
     DESTINATION ${EXECUTABLE_OUTPUT_PATH})
file(COPY ${PROJECT_SOURCE_DIR}/data/minimal/luna_pinyin.dict.yaml
     DESTINATION ${EXECUTABLE_OUTPUT_PATH})
file(COPY ${PROJECT_SOURCE_DIR}/data/minimal/luna_pinyin.schema.yaml
     DESTINATION ${EXECUTABLE_OUTPUT_PATH})
file(COPY ${PROJECT_SOURCE_DIR}/data/minimal/cangjie5.dict.yaml
     DESTINATION ${EXECUTABLE_OUTPUT_PATH})
file(COPY ${PROJECT_SOURCE_DIR}/data/minimal/cangjie5.schema.yaml
     DESTINATION ${EXECUTABLE_OUTPUT_PATH})
