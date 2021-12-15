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

#include "esp_log.h"
#include "esp_system.h"
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "net_certs.h"

char* g_client_cert = NULL;
char* g_client_key = NULL;
char* g_ca_cert = NULL;

static const char *TAG = "net_certs";

esp_err_t net_certs_load(char* filename, char** buf)
{
  struct stat st;

  if (stat(filename, &st) == -1) {
    ESP_LOGE(TAG, "can't stat %s", filename);
    return -1;
  }

  int fd = open(filename, O_RDONLY);
  if (fd == -1) {
    ESP_LOGE(TAG, "can't open %s", filename);
    return -1;
  }

  *buf = malloc(st.st_size);
  if (*buf == NULL) {
    ESP_LOGE(TAG, "can't malloc %ld bytes to load %s", (long)st.st_size, filename);
    close(fd);
    return -1;
  }

  int r = read(fd, *buf, 1);
  if (r != st.st_size) {
    ESP_LOGE(TAG, "couldn't read %ld bytes from %s", (long)st.st_size, filename);
    free(*buf);
    close(fd);
    return -1;
  }

  close(fd);
  ESP_LOGI(TAG, "Loaded certificate (%ld bytes) from %s", (long)st.st_size, filename);
  return ESP_OK;
}

esp_err_t net_certs_init()
{
  if (net_certs_load("/certs/ca.crt", &g_ca_cert) != ESP_OK) {
    g_ca_cert = NULL;
  }

  if (net_certs_load("/certs/client.crt", &g_client_cert) != ESP_OK) {
    g_client_cert = NULL;
  }

  if (net_certs_load("/certs/client.key", &g_client_key) != ESP_OK) {
    g_client_key = NULL;
  }

  if (g_ca_cert == NULL || g_client_cert == NULL || g_client_key == NULL) {
    ESP_LOGE(TAG, "One or more certificates failed to load!");
    return -1;
  }

  return ESP_OK;
}
