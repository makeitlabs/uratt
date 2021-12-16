#ifndef _CONSOLE_H
#define _CONSOLE_H

void console_init(void);
int console_poll(void);
void console_done(void);


void console_register_cmd_log(void);
void console_register_cmd_nvs_dump(void);
void console_register_cmd_nvs_set(void);

#endif
