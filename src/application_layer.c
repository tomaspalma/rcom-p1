// Application layer protocol implementation

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "application_layer.h"
#include "link_layer.h"
#include "utils.h"

unsigned char *read_control_frame(int app_layer_control, int *file_size) {
  if (*file_size != 0) {
    printf("Initial value of file size needs to be 0\n");
    return NULL;
  }

  unsigned char buf[BUFSIZ];

  if (llread(&buf) == 0) {
    return NULL;
  }

  if (buf[0] != app_layer_control) {
    printf("App layer control wrong\n");
    return NULL;
  }

  // unsigned char buf = 0;
  unsigned char *filename = NULL;

  int l = INVALID;
  int stop = 0;
  int index = 1;
  READ_CONTROL_STATE state = READ_FILESIZE;
  while (!stop) {
    if (buf[index] == 0 && state == READ_FILESIZE) {
      index++;
      l = buf[index++];

      printf("L is: %d\n", l);

      int a = l;
      for (; a > 0; a--) {
        printf("Index is: %d\n", index);
        *file_size |= (buf[index] << (8 * (l - a)));
        index++;
      }

      printf("FILE SIZE RECEIVER WILL BE: %d\n", *file_size);
      state = READ_FILENAME;
      printf("Index is: %d\n", index);
    } else if (buf[index] == 1 && state == READ_FILENAME) {
      index++;
      printf("Index is: %d\n", index);
      l = buf[index++];

      printf("L is: %d\n", buf[6]);

      filename = (unsigned char *)malloc(l);

      for (int i = 0; i < l; i++) {
        filename[i] = buf[index + i];
      }

      printf("Filename is: %s\n", filename);

      stop = 1;
    }
  }

  return filename;
}

int read_file(int file_size, unsigned char *filename, FILE *file,
              int *current_size) {
  if (filename == NULL) {
    printf("Filename cannot be null\n");
    return -1;
  }

  unsigned char buf[BUFSIZ];

  int l1 = 0;
  int l2 = 0;

  printf("------------------------------\n", file_size);
  printf("File size: %d\n", file_size);
  printf("------------------------------\n");

  int packets = 0;
  int packet_size;
  int bytes_read = 0;
  int current_bytes_read;
  while (bytes_read < file_size) {
    llread(&buf);

    l2 = buf[1];
    l1 = buf[2];
    packet_size = l2;
    packet_size <<= 8;
    packet_size |= l1;

    bytes_read += packet_size;

    printf("Packet Size: %d\n", packet_size);

    packets++;

    printf("Before segmenetation    \n");
    for (int i = 0; i < packet_size; i++) {
      fputc(buf[3 + i], file);
    }
    printf("After segmenetation    \n");
  }

  // unsigned char buf[file_size];
  // unsigned char file_buf[file_size + 1];
  //
  // int file_index = 0;
  // int b = 0;
  // int c;
  // int debugy = 0;
  // while (1) {
  //   memset(buf, 0, file_size);
  //   c = llread(&buf);
  //   b += c;
  //
  //   printf("WHAT THE ACTUAL FUCK: %x\n", buf[0]);
  //
  //   printf("B is: %d\n", b);
  //
  //   if (buf[0] != CONTROL_DATA) {
  //     printf("App layer data control field wrong");
  //     return -1;
  //   }
  //
  //   printf("We have read: %d\n", c);
  //   // fputs(&buf[3], file);
  //   printf("Foda-se: %d\n", strlen(buf + 3));
  //   printf("Foda-se2: %d\n", c);
  //   for (int i = 0; i < c; i++) {
  //     printf("Reeived UWU: %c\n", buf[i + 3]);
  //     fputc(buf[i + 3], file);
  //   }
  //   debugy += (c - 3);
  //   // printf("File index: %d\n", file_index);
  //   // strcpy(buf[3], file_buf + file_index);
  //   // file_index += c;
  //
  //   if (c == 10968) {
  //     printf("C is: %d\n", c);
  //     printf("WE HARDOCODING BRO: %d\n", debugy);
  //     break;
  //   }
  // }
  //
  // printf("Exited llread\n");
  //
  // READ_FILE_STATE read_state = CONTROL_DATA;
  //
  // int packet_size = 0;

  // int l2 = buf[1];
  // int l1 = buf[2];
  // *current_size += 256 * l2 + l1;
  //
  // if (get_size_of_file(file) != *current_size) {
  //   printf("Number of bytes supposed to have been received is different than
  //   "
  //          "the number of bytes that was actually received");
  // }

  printf("PACKETS READ WERE: %d\n", packets);

  printf("Leaving read file!!!\n");

  return 0;
}

int receiver_application_layer() {
  unsigned char *filename = NULL;

  int file_size_start = 0;
  if ((filename = read_control_frame(CONTROL_START, &file_size_start)) ==
      NULL) {
    printf("");
    return -1;
  }

  int current_size = 0;
  FILE *file;
  if ((file = fopen("penguin_received.gif", "w")) == NULL) {
    printf("Failed to open file!");
    return -1;
  }
  printf("Reading file: \n");
  if (read_file(file_size_start, filename, file, &current_size) == -1) {
    return -1;
  }
  printf("Stopped reading file: \n");

  free(filename);
  filename = NULL;
  int file_size_stop = 0;
  if ((filename = read_control_frame(CONTROL_END, &file_size_stop)) == NULL) {
    printf("");
    return -1;
  }
}

int send_file(FILE *file, int file_size) {
  printf("Started trying to send file!\n");
  if (file == NULL) {
    printf("One or more of the arguments passed are NULL");
    return -1;
  }

  int packets = 0;
  unsigned char packet[3 + MAX_DATAFIELD_SIZE];
  packet[0] = CONTROL_DATA;
  int bytes_read;
  int total_read = 0;

  while ((bytes_read = fread(packet + 3, sizeof(unsigned char),
                             MAX_DATAFIELD_SIZE, file)) >= MAX_DATAFIELD_SIZE) {
    packet[1] = (bytes_read & 0xff00) >> 8;
    packet[2] = bytes_read & 0xff;
    for (int i = 0; i < bytes_read; i++) {
      printf("God help please :( %c\n", packet[3 + i]);
      printf("Bytes read are: %d\n", bytes_read);
      printf("Packet 0 is: %x\n", packet[0]);
    }

    if (llwrite(packet, bytes_read + 3) == -1) {
      return -1;
    }
    total_read += bytes_read;
    packets++;
  }

  packet[1] = (bytes_read & 0xff00) >> 8;
  packet[2] = bytes_read & 0xff;
  if (llwrite(packet, bytes_read + 3) == -1) {
    return -1;
  }
  packets++;

  printf("<<<<<<<<<<>>>>>>>>>>>> Total read from file: %d\n",
         total_read + bytes_read);

  printf("Packets were: %d\n", packets);

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
  if (file == NULL) {
    printf("Error opening file %s\n", filename);
    return -1;
  }

  int file_size = get_size_of_file(file);
  if (send_control_frame(filename, file_size, CONTROL_START) == -1) {
    return -1;
  }

  printf("----------------------------- Sending file\n");
  if (send_file(file, file_size) == -1) {
    return -1;
  }
  printf("Stopped sending file---------------------------------------\n");

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
    if (transmitter_application_layer(filename) == -1) {
      printf("Transmitting application layer failed!\n");
      return -1;
    }
    // unsigned char msg[6] = {'h', 'e', 0x7d, 'l', 0x7e, 'o'};
    // int bcc = 0;
    // for (int i = 0; i < 6; i++) {
    //   bcc ^= msg[i];
    // }
    // printf("Bcc should be: %d\n", bcc);
    // llwrite(msg, 6);
    // llwrite(msg, 6);
    // llwrite(msg, 6);
    // llwrite(msg, 6);
    // llwrite(msg, 6);
  } else {
    if (receiver_application_layer() == -1) {
      printf("Receiving application layer failed!\n");
      return -1;
    }
    // unsigned char msg[6];
    // llread(msg);
    // printf("%s\n", msg);
    // unsigned char m[6];
    // llread(m);
    // printf("%s\n", m);
    // unsigned char s[6];
    // llread(s);
    // printf("%s\n", s);
    // unsigned char a[6];
    // llread(a);
    // printf("%s\n", a);
    // unsigned char b[6];
    // llread(b);
    // printf("%s\n", b);
  }

  if (llclose(conn_params.role, 1) == -1) {
    printf("Failed to restore previous configuration on serial port\n");
    return;
  }
}
