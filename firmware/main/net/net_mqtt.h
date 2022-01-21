#ifndef _NET_MQTT
#define _NET_MQTT

#include "display_task.h"

int net_mqtt_init(void);
int net_mqtt_start(void);
int net_mqtt_stop(void);

void net_mqtt_send_boot_status(void);
void net_mqtt_send_wifi_strength(void);
void net_mqtt_send_acl_updated(char* status);
void net_mqtt_send_access(char *member, int allowed);
void net_mqtt_send_access_error(char *err_text, char *err_ext);
void net_mqtt_send_power_status(power_status_t status);
void net_mqtt_send_door_state(bool door_open);
void net_mqtt_send_ota_status(ota_status_t status, int progress);

#define MQTT_BASE_TOPIC "ratt"
#define MQTT_TOPIC_TYPE_STATUS "status"
#define MQTT_TOPIC_TYPE_CONTROL "control"

#define MQTT_ACL_SUCCESS "downloaded"
#define MQTT_ACL_FAIL "failed"


#endif
