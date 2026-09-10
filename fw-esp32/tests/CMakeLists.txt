cmake_minimum_required(VERSION 3.16)

# Standalone Unity test app for the P5 target gate. It reuses the gateway
# components through EXTRA_COMPONENT_DIRS but restricts COMPONENTS to the
# link/protocol pieces under test, so the Wi-Fi/TCP adapters (and their
# component-manager mdns dependency) are not built or started.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components")
set(COMPONENTS main la_board la_link la_proto)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(la_gateway_tests)
