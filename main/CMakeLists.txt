set(INC_DIRS "./")
file(GLOB_RECURSE C_SRCS "./*.c")

idf_component_register(SRCS ${C_SRCS} INCLUDE_DIRS ${INC_DIRS})