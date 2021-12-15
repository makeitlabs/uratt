idf_component_register(
  SRCS
  "audio_task.c"
  "beep_task.c"
  "display_task.c"
  "door_task.c"
  "glcdfont.c"
  "https.c"
  "i2c_periph.c"
  "lcd_st7735.c"
  "main.c"
  "main_task.c"
  "net_https.c"
  "net_mqtt.c"
  "net_ota.c"
  "net_sntp.c"
  "net_task.c"
  "rfid_task.c"
  "sdcard.c"
  "system_task.c"
  INCLUDE_DIRS ".")

target_add_binary_data(${COMPONENT_TARGET} "ca.crt" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "client.crt" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "client.key" TEXT)
