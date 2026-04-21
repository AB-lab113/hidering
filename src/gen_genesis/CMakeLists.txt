set(gen_genesis_sources gen_genesis.cpp)

monero_add_executable(gen_genesis ${gen_genesis_sources})
target_link_libraries(gen_genesis
  PRIVATE
    cryptonote_core
    common
    cncrypto
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT}
    ${EXTRA_LIBRARIES})
set_property(TARGET gen_genesis PROPERTY OUTPUT_NAME "hidering-gen-genesis")
