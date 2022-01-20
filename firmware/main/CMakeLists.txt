file(GLOB_RECURSE SOURCES *.c )
file(GLOB_RECURSE NET_SOURCES net/*.c )
file(GLOB_RECURSE SYSTEM_SOURCES system/*.c )
file(GLOB_RECURSE UI_SOURCES ui/*.c )
file(GLOB_RECURSE IMAGE_SOURCES ui/images/*.c)
file(GLOB_RECURSE FONT_SOURCES ui/fonts/*.c)

set (include_dirs "./" "net" "system" "ui" "ui/images")

idf_component_register(
  SRCS
  ${SOURCES}
  ${NET_SOURCES}
  ${SYSTEM_SOURCES}
  ${UI_SOURCES}
  ${IMAGE_SOURCES}
  ${FONT_SOURCES}
  INCLUDE_DIRS "."
  INCLUDE_DIRS "${include_dirs}"
  )
