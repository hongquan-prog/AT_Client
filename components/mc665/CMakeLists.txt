file(GLOB_RECURSE C_SRCS "./*.c")

set(INC_DIRS "./")

idf_component_register(SRCS ${C_SRCS} INCLUDE_DIRS ${INC_DIRS} PRIV_REQUIRES at_client interface)