idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash fatfs wear_levelling
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
)
