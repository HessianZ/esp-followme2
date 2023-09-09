idf_component_register(
        SRC_DIRS
        "."
        "app"
        "gui"
        "gui/font"
        "gui/image"

        INCLUDE_DIRS
        "."
        "app"
        "gui"

        EMBED_FILES root.html
)

file(GLOB_RECURSE FOLLOWME2_SOURCES ./*.c)

set_source_files_properties(
        ${FOLLOWME2_SOURCES}
        PROPERTIES COMPILE_OPTIONS
        -DLV_LVGL_H_INCLUDE_SIMPLE)