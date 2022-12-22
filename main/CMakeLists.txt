# aux_source_directory(app C_SRCS)
# aux_source_directory(driver C_SRCS)
# aux_source_directory(interface C_SRCS)
file(GLOB_RECURSE C_SRCS   "main.c" "driver/*.c" "interface/*.c" "app/*.c")
set(INC_DIRS "app" "driver" "interface")

idf_component_register(SRCS ${C_SRCS} INCLUDE_DIRS ${INC_DIRS})