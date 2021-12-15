#ifndef _CONSOLE_H
#define _CONSOLE_H

void console_init(void);
void console_register_cmd_log(void);
int console_poll(void);
void console_done(void);


#endif
