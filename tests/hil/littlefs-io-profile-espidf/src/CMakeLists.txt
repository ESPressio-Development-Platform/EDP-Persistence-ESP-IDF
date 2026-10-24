idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash joltwallet__littlefs
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
target_include_directories(
    ${COMPONENT_LIB}
    PRIVATE
      "../../../../src"
      "../../../../../EDP-Persistence/src"
      "../../../../../EDP-Memory/src"
      "../../../../../EDP-Platform/src"
      "../../../../../EDP-Platform-Portable/src"
      "../../../../../EDP-System/src"
      "../../../../../EDP-BoundedTopology/src"
      "../../../../../EDP-BoundedTypes/src"
)

# HIL-only flash-operation instrumentation; no production library change.
# Count vendor LittleFS's calls into native partition write/erase APIs.
target_link_options(${COMPONENT_LIB} INTERFACE
    "-Wl,--wrap=esp_partition_write"
    "-Wl,--wrap=esp_partition_erase_range"
)
