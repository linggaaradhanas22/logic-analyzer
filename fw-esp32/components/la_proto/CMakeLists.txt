# Wrapper component that compiles the shared, allocation-free protocol library
# (common/la_proto) for the ESP32 gateway. The Zynq firmware and the host tool
# use the same sources; no copies exist in fw-esp32.

set(LA_PROTO_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../common/la_proto")

idf_component_register(
    SRCS "${LA_PROTO_DIR}/src/la_proto.c"
    INCLUDE_DIRS "${LA_PROTO_DIR}/include"
)
