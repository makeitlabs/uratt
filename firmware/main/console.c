/*--------------------------------------------------------------------------
  _____       ______________
 |  __ \   /\|__   ____   __|
 | |__) | /  \  | |    | |
 |  _  / / /\ \ | |    | |
 | | \ \/ ____ \| |    | |
 |_|  \_\/    \_\_|    |_|    ... RFID ALL THE THINGS!

 A resource access control and telemetry solution for Makerspaces

 Developed at MakeIt Labs - New Hampshire's First & Largest Makerspace
 http://www.makeitlabs.com/

 Copyright 2017-2020 MakeIt Labs

 Permission is hereby granted, free of charge, to any person obtaining a
 copy of this software and associated documentation files (the "Software"),
 to deal in the Software without restriction, including without limitation
 the rights to use, copy, modify, merge, publish, distribute, sublicense,
 and/or sell copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 --------------------------------------------------------------------------
 Author: Steve Richardson (steve.richardson@makeitlabs.com)
 -------------------------------------------------------------------------- */

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_vfs_dev.h"

#include "esp_log.h"
#include "esp_console.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "console.h"

static char* prompt = LOG_COLOR_I "uRATT> " LOG_RESET_COLOR;

void console_init(void)
{
    /* Drain stdout before reconfiguring it */
    fflush(stdout);
    fsync(fileno(stdout));

    /* Disable buffering on stdin */
    setvbuf(stdin, NULL, _IONBF, 0);

    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    esp_vfs_dev_uart_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    esp_vfs_dev_uart_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CRLF);

    /* Configure UART. Note that REF_TICK is used so that the baud rate remains
     * correct while APB frequency is changing in light sleep mode.
     */
    const uart_config_t uart_config = {
            .baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .source_clk = UART_SCLK_REF_TICK,
    };

    /* Install UART driver for interrupt-driven reads and writes */
    ESP_ERROR_CHECK( uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0) );
    ESP_ERROR_CHECK( uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config) );

    /* Tell VFS to use UART driver */
    esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

    /* Initialize the console */
    esp_console_config_t console_config = {
            .max_cmdline_args = 8,
            .max_cmdline_length = 256,
            .hint_color = atoi(LOG_COLOR_CYAN)
    };
    ESP_ERROR_CHECK( esp_console_init(&console_config) );

    /* Configure linenoise line completion library */
    /* Enable multiline editing. If not set, long commands will scroll within
     * single line.
     */
    linenoiseSetMultiLine(1);

    /* Tell linenoise where to get command completions and hints */
    linenoiseSetCompletionCallback(&esp_console_get_completion);
    linenoiseSetHintsCallback((linenoiseHintsCallback*) &esp_console_get_hint);

    /* Set command history size */
    linenoiseHistorySetMaxLen(100);

    /* Set command maximum length */
    linenoiseSetMaxLineLen(console_config.max_cmdline_length);

    /* Don't return empty lines */
    linenoiseAllowEmpty(false);

    /* register help command */
    esp_console_register_help_command();

    console_register_cmd_log();
    console_register_cmd_nvs_dump();
    console_register_cmd_nvs_set();

    printf("\n\n"
           "Welcome to MakeIt Labs uRATT - RFID All The Things!\n"
           "Type 'help' to get the list of commands.\n"
           "Use UP/DOWN arrows to navigate through command history.\n"
           "Press TAB when typing command name to auto-complete.\n");

    /* Figure out if the terminal supports escape sequences */
    int probe_status = linenoiseProbe();
    if (probe_status) { /* zero indicates success */
       printf("\n"
              "Your terminal application does not support escape sequences.\n"
              "Line editing and history features are disabled.\n");
       linenoiseSetDumbMode(1);
       prompt = "uRATT> ";
    }
}

void console_done(void)
{
  esp_console_deinit();
}


static struct {
    struct arg_str *tag;
    struct arg_str *level;
    struct arg_end *end;
} log_args;

static int set_log(int argc, char **argv)
{
  esp_log_level_t esp_log_level = ESP_LOG_ERROR;

  int nerrors = arg_parse(argc, argv, (void **) &log_args);
  if (nerrors != 0) {
      arg_print_errors(stderr, log_args.end, argv[0]);
      return 1;
  }

  if (strlen(log_args.tag->sval[0]) >= 1 && strlen(log_args.level->sval[0]) >= 1) {
    char firstchar = toupper(log_args.level->sval[0][0]);

    switch (firstchar) {
    case 'N':
      esp_log_level = ESP_LOG_NONE;
      break;
    case 'E':
      esp_log_level = ESP_LOG_ERROR;
      break;
    case 'W':
      esp_log_level = ESP_LOG_WARN;
      break;
    case 'I':
      esp_log_level = ESP_LOG_INFO;
      break;
    case 'D':
      esp_log_level = ESP_LOG_DEBUG;
      break;
    case 'V':
      esp_log_level = ESP_LOG_VERBOSE;
      break;
    default:
      printf("\nInvalid log level\n");
    }

    esp_log_level_set(log_args.tag->sval[0], esp_log_level);

    printf("\nSet log level %s to %s\n\n", log_args.tag->sval[0], log_args.level->sval[0]);
    return ESP_OK;
  } else {
    printf("\nMust specify <tag> <level>\n");
  }

  return ESP_ERR_NOT_FOUND;
}

static struct {
    struct arg_str *namespace;
    struct arg_end *end;
} nvs_dump_args;

static int nvs_dump(int argc, char **argv)
{
  int nerrors = arg_parse(argc, argv, (void **) &nvs_dump_args);
  if (nerrors != 0) {
      arg_print_errors(stderr, nvs_dump_args.end, argv[0]);
      return 1;
  }

  nvs_handle_t hdl;
  esp_err_t r = nvs_open(nvs_dump_args.namespace->sval[0], NVS_READONLY, &hdl);
  if (r != ESP_OK) {
    printf("\nNVS open error: %s\n", esp_err_to_name(r));
    return r;
  }

  // Example of listing all the key-value pairs of any type under specified partition and namespace
  nvs_iterator_t it = nvs_entry_find("nvs", nvs_dump_args.namespace->sval[0], NVS_TYPE_ANY);
  while (it != NULL) {
      nvs_entry_info_t info;
      nvs_entry_info(it, &info);
      it = nvs_entry_next(it);
      printf("%s.%s=", info.namespace_name, info.key);
      switch(info.type) {
        case NVS_TYPE_STR:
          {
            char s[128];
            size_t len = sizeof(s);
            nvs_get_str(hdl, info.key, s, &len);
            printf("\"%s\"", s);
          }
          break;
        default:
          printf("[no value handler]");
      }
      printf("\n");
  };

  nvs_close(hdl);

  return ESP_OK;
}


static struct {
    struct arg_str *namespace;
    struct arg_str *key;
    struct arg_str *value;
    struct arg_end *end;
} nvs_set_args;
static int nvs_set(int argc, char **argv)
{
  int nerrors = arg_parse(argc, argv, (void **) &nvs_set_args);
  if (nerrors != 0) {
      arg_print_errors(stderr, nvs_set_args.end, argv[0]);
      return 1;
  }

  nvs_handle_t hdl;
  esp_err_t r = nvs_open(nvs_set_args.namespace->sval[0], NVS_READWRITE, &hdl);
  if (r != ESP_OK) {
    printf("\nNVS open error: %s\n", esp_err_to_name(r));
    return r;
  }

  r = nvs_set_str(hdl, nvs_set_args.key->sval[0], nvs_set_args.value->sval[0]);

  if (r != ESP_OK) {
    printf("\nNVS set error: %s", esp_err_to_name(r));
    nvs_close(hdl);
    return r;
  }

  nvs_close(hdl);
  return ESP_OK;
}



void console_register_cmd_log(void)
{
    log_args.tag = arg_str1(NULL, NULL, "<tag>", "TAG of module to change, * to reset all to a given level");
    log_args.level = arg_str1(NULL, NULL, "<level>", "Log level to set (NONE, ERROR, WARN, INFO, DEBUG, VERBOSE)");
    log_args.end = arg_end(2);

    const esp_console_cmd_t log_cmd = {
        .command = "log",
        .help = "Set log level for a given TAG",
        .hint = NULL,
        .func = &set_log,
        .argtable = &log_args
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&log_cmd) );
}


void console_register_cmd_nvs_dump(void)
{
  nvs_dump_args.namespace = arg_str1(NULL, NULL, "<namespace>", "NVS namespace");
  nvs_dump_args.end = arg_end(2);

  const esp_console_cmd_t nvs_dump_cmd = {
      .command = "nvs_dump",
      .help = "Dump all NVS variables in a namespace",
      .hint = NULL,
      .func = &nvs_dump,
      .argtable = &nvs_dump_args
  };

  ESP_ERROR_CHECK( esp_console_cmd_register(&nvs_dump_cmd) );
}


void console_register_cmd_nvs_set(void)
{
  nvs_set_args.namespace = arg_str1(NULL, NULL, "<namespace>", "NVS namespace");
  nvs_set_args.key = arg_str1(NULL, NULL, "<key>", "item key name");
  nvs_set_args.value = arg_str1(NULL, NULL, "<value>", "item value (string)");
  nvs_set_args.end = arg_end(2);

  const esp_console_cmd_t nvs_dump_cmd = {
      .command = "nvs_set",
      .help = "Set NVS key:value pair in namespace",
      .hint = NULL,
      .func = &nvs_set,
      .argtable = &nvs_set_args
  };

  ESP_ERROR_CHECK( esp_console_cmd_register(&nvs_dump_cmd) );
}



int console_poll(void)
{
    char* line = linenoise(prompt);
    if (line == NULL) { /* Break on EOF or error */
        return 1;
    }
    if (strlen(line) > 0) {
        linenoiseHistoryAdd(line);
    }

    int ret;
    esp_err_t err = esp_console_run(line, &ret);
    if (err == ESP_ERR_NOT_FOUND) {
        printf("Unrecognized command\n");
    } else if (err == ESP_ERR_INVALID_ARG) {
        // command was empty
    } else if (err == ESP_OK && ret != ESP_OK) {
        printf("Command returned non-zero error code: 0x%x (%s)\n", ret, esp_err_to_name(ret));
    } else if (err != ESP_OK) {
        printf("Internal error: %s\n", esp_err_to_name(err));
    }
    /* linenoise allocates line buffer on the heap, so need to free it */
    linenoiseFree(line);

    return 0;
}
