// Application layer protocol implementation

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "application_layer.h"
#include "link_layer.h"
#include "utils.h"

int send_file(FILE *file) {
  if (file == NULL) {
    printf("One or more of the arguments passed are NULL");
    return -1;
  }

  unsigned char packet[3 + MAX_DATAFIELD_SIZE];
  packet[0] = CONTROL_DATA;
  int bytes_read;
  while ((bytes_read = fread(packet + 3, 1, MAX_DATAFIELD_SIZE, file)) > 0) {
    packet[1] = (bytes_read & 0xff00) >> 8;
    packet[2] = bytes_read & 0xff;
    for (int i = 0; i < bytes_read; i++) {
      if (llwrite(packet + 3, bytes_read) == -1) {
        return -1;
      }
    }
  }
}

int send_control_frame(const char *filename, int file_size,
                       int app_layer_control) {
  if (filename == NULL) {
    printf("Filename passed as argument is NULL\n");
    return -1;
  }

  unsigned char filename_len = strlen(filename);
  unsigned char file_size_bytes = ((get_no_of_bits(file_size) + 7) / 8);
  int buf_size = 5 + filename_len + file_size_bytes;
  unsigned char control[buf_size];
  memset(control, 0, buf_size);

  control[0] = app_layer_control;

  // File size
  control[1] = FILE_SIZE;
  control[2] = file_size_bytes;

  printf("File size: %d\n", file_size);
  printf("File size bytes: %d\n", file_size_bytes);
  for (int i = 0; i < file_size_bytes; i++) {
    control[3 + i] = (file_size & 0xff);
    file_size >>= 8;
  }

  unsigned char index = file_size_bytes + 2 + 1;
  control[index] = FILE_NAME;
  control[index + 1] = filename_len;
  strcpy(control + index + 2, filename);

  if (llwrite(control, buf_size) == -1) {
    printf("Llwrite failed on trying to send the control frame for start\n");
    return -1;
  }

  return 0;
}

int transmitter_application_layer(const char *filename) {
  FILE *file = fopen(filename, "r");

  int file_size = get_size_of_file(file);
  if (send_control_frame(filename, file_size, CONTROL_START) == -1) {
    return -1;
  }

  if (send_file(filename) == -1) {
    return -1;
  }

  if (send_control_frame(filename, file_size, CONTROL_END) == -1) {
    return -1;
  }
}

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
    return;
  }

  if (conn_params.role == LlTx) {
    /*if (transmitter_application_layer(filename) == -1) {
      return -1;
    }*/

    unsigned char *msg = "hello";
    llwrite(msg, 5);
    llwrite(msg, 5); /*if(send_file(filename) == -1) {

   return;
 }*/
  } else {
    unsigned char msg[5];
    llread(msg);
    printf("%s\n", msg);
    unsigned char m[5];
    llread(m);
    printf("%s\n", m);
  }
}
