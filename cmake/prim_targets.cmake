add_executable(bradv1 ${PRIM_DIR}/demo-attacks/bradv1.c)
add_executable(smart-lock ${PRIM_DIR}/demo-attacks/smart-lock.c)
add_executable(bradv2 ${PRIM_DIR}/demo-attacks/bradv2.c)

add_dump_target(bradv1)
add_dump_target(smart-lock)
add_dump_target(bradv2)