// Application layer protocol implementation

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "application_layer.h"
#include "link_layer.h"

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename) {
  if (serialPort == NULL || role == NULL || filename == NULL) {
    printf("Some parameters passed are null\n");
    return;
  } else if (timeout < 0) {
    printf("Invalid timeout value\n");
    return;
  }

  LinkLayer conn_params;

  if (strcmp("rx", role) == 0) {
    conn_params.role = LlRx;
  } else if (strcmp("tx", role) == 0) {
    conn_params.role = LlTx;
  } else {
    printf("Invalid role\n");
    return;
  }

  strcpy(conn_params.serialPort, serialPort);
  conn_params.baudRate = baudRate;
  conn_params.timeout = timeout;
  conn_params.nRetransmissions = nTries;

  if (llopen(conn_params) == -1) {
  }

  printf("??????????????\n");
}
