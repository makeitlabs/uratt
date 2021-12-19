file(GLOB_RECURSE IMAGE_SOURCES images/*.c)
file(GLOB_RECURSE SOURCES *.c )

idf_component_register(
  SRCS
  ${SOURCES}
  ${IMAGE_SOURCES}
  INCLUDE_DIRS ".")
