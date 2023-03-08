function (gen_settings_proto)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    set(VENV ${CMAKE_CURRENT_BINARY_DIR}/venv)
    set(VENV_FILE ${VENV}/environment.txt)
    set(VENV_BIN_DIR ${VENV}/Scripts)

    add_custom_command(
        DEPENDS ${CMAKE_SOURCE_DIR}/../lib/nanopb/extra/requirements.txt
        COMMAND ${Python3_EXECUTABLE} -m venv ${VENV}
        COMMAND ${VENV_BIN_DIR}/pip --disable-pip-version-check install -r ${CMAKE_SOURCE_DIR}/../lib/nanopb/extra/requirements.txt
        COMMAND ${VENV_BIN_DIR}/pip freeze > ${VENV_FILE}
        OUTPUT ${VENV_FILE}
        COMMENT "Setting up Python Virtual Environment"
    )

    set(NANOPB_GENERATOR ${CMAKE_SOURCE_DIR}/../lib/nanopb/generator/nanopb_generator.py)
    set(PROTO_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/proto)
    set(PROTO_OUTPUT_DIR ${PROTO_OUTPUT_DIR} PARENT_SCOPE)

    add_custom_command(
        DEPENDS ${VENV_FILE} ${NANOPB_GENERATOR} proto/settings.proto
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PROTO_OUTPUT_DIR}
        COMMAND ${VENV_BIN_DIR}/python ${NANOPB_GENERATOR} -D ${PROTO_OUTPUT_DIR} -I proto proto/settings.proto
        OUTPUT ${PROTO_OUTPUT_DIR}/settings.pb.c ${PROTO_OUTPUT_DIR}/settings.pb.h
        COMMENT "Generating code for settings"
    )
endfunction()
