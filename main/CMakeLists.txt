idf_component_register(
        SRC_DIRS
        "."
        "app"
        "gui"
        "gui/font"
        "gui/image"
        "gui/page"

        INCLUDE_DIRS
        "."
        "app"
        "gui"
        "gui/page"
)

file(GLOB_RECURSE FOLLOWME2_SOURCES ./*.c)

set_source_files_properties(
        ${FOLLOWME2_SOURCES}
        PROPERTIES COMPILE_OPTIONS
        -DLV_LVGL_H_INCLUDE_SIMPLE)

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)