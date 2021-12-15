#ifndef _NET_CERTS_H
#define _NET_CERTS_H

char* g_client_cert;
char* g_client_key;
char* g_ca_cert;

esp_err_t net_certs_init();

#endif
