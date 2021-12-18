file(GLOB_RECURSE IMAGE_SOURCES images/*.c)

idf_component_register(
  SRCS
  "beep_task.c"
  "config.c"
  "console.c"
  "display_lvgl.c"
  "display_task.c"
  "door_task.c"
  "esp_lcd_panel_st7735.c"
  "https.c"
  "i2c_periph.c"
  "lvgl_demo_ui.c"
  "main.c"
  "main_task.c"
  "net_certs.c"
  "net_https.c"
  "net_mqtt.c"
  "net_ota.c"
  "net_sntp.c"
  "net_task.c"
  "rfid_task.c"
  "sdcard.c"
  "spiflash.c"
  "system_task.c"
  ${IMAGE_SOURCES}
  INCLUDE_DIRS ".")
