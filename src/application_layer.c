// Application layer protocol implementation

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "application_layer.h"
#include "link_layer.h"
#include "utils.h"

extern int fd;

int read_control_frame(int app_layer_control, int *file_size,
                       unsigned char *filename) {
  if (*file_size != 0) {
    printf("Initial value of file size needs to be 0\n");
    return -1;
  } else if (filename != NULL) {
    printf("Filename must be NULL");
    return -1;
  }

  // unsigned char buf = 0;

  int stop = 0;

  int t = INVALID;
  int l = INVALID;
  int length_read = 0;
  int v_left_to_read = 0;

  unsigned char buf[BUFSIZ];

  if (llread(&buf) == 0) {
    return -1;
  }

  if (buf[0] != app_layer_control) {
    printf("App layer control wrong\n");
    return -1;
  }

  READ_CONTROL_STATE state = READ_FILESIZE;
  int index = 1;
  while (!stop) {
    if (buf[index++] == 0 && state == READ_FILESIZE) {
      l = buf[index++];

      int a = l;
      for (int i = index; i < l + index; i++) {
        *file_size |= (buf[index] << (8 * (l - a)));
        a--;
        index++;
      }

      printf("FILE SIZE RECEIVER WILL BE: %d\n", *file_size);

      state = READ_FILENAME;
    } else if (buf[index++] == 1 && state == READ_FILENAME) {
      l = buf[index++];

      for (int i = 0; i < l; i++) {
        filename[i] = buf[index + i];
      }

      stop = 1;
    }
  }

  return 0;
}

int read_file(int file_size, unsigned char *filename) {
  if (filename == NULL) {
    printf("Filename cannot be null\n");
    return -1;
  }

  unsigned char buf = 0;
  int bytes_read = 0;
  READ_FILE_STATE read_state = CONTROL_DATA;

  int l2 = 0;
  int l1 = 0;
  int packet_size = 0;
  int k = 0;

  int stop = 0;
  while (!stop) {
    read(fd, &buf, 1);

    if (read_state == READ_CONTROL_DATA) {
      if (buf == CONTROL_DATA) {
        read_state = READ_DATA_L2;
      }
    } else if (read_state == READ_DATA_L2) {
      l2 = buf;
      read_state = READ_DATA_L1;
    } else if (read_state == READ_DATA_L1) {
      l1 = buf;

      packet_size |= ((l2 << 8) | l1);
      k = 256 * l2 + l1;
      read_state = READ_DATA;
    } else if (read_state == READ_DATA) {
    }
  }

  return 0;
}

int receiver_application_layer() {
  unsigned char *filename = NULL;

  int file_size_start = 0;
  if (read_control_frame(CONTROL_START, &file_size_start, filename) == -1) {
    printf("");
    return -1;
  }

  if (read_file(file_size_start, filename) == -1) {
    return -1;
  }

  free(filename);
  filename = NULL;
  int file_size_stop = 0;
  if (read_control_frame(CONTROL_END, &file_size_stop, filename) == -1) {
    printf("");
    return -1;
  }
}

int send_file(FILE *file) {
  if (file == NULL) {
    printf("One or more of the arguments passed are NULL");
    return -1;
  }

  unsigned char packet[10969];
  packet[0] = CONTROL_DATA;
  int bytes_read;

  fread(packet, sizeof(unsigned char), 10969, file);
  // while ((bytes_read = fread(packet, sizeof(unsigned char), 4, file)) > 0) {
  //   printf("Bytes read: %d\n", bytes_read);
  //   packet[1] = (bytes_read & 0xff00) >> 8;
  //   packet[2] = bytes_read & 0xff;
  //   if (llwrite(packet + 3, bytes_read) == -1) {
  //     return -1;
  //   }
  // }
  return 0;
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

  printf(":((((((((((()))))))))))\n");

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
  if (file == NULL) {
    printf("Error opening file %s\n", filename);
    return -1;
  }

  printf("A\n");
  int file_size = get_size_of_file(file);
  printf("File size: %d\n", file_size);
  if (send_control_frame(filename, file_size, CONTROL_START) == -1) {
    return -1;
  }

  if (send_file(file) == -1) {
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
    // if (transmitter_application_layer(filename) == -1) {
    //   printf("Transmitting application layer failed!\n");
    //   return -1;
    // }
    unsigned char *msg = "hello";
    llwrite(msg, 5);
    llwrite(msg, 5);
  } else {
    // if (receiver_application_layer() == -1) {
    //   printf("Receiving application layer failed!\n");
    //   return -1;
    // }
    unsigned char msg[5];
    llread(msg);
    printf("%s\n", msg);
    unsigned char m[5];
    llread(m);
    printf("%s\n", m);
  }
}
