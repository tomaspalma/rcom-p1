// Application layer protocol header.
// NOTE: This file must not be changed.

#ifndef _APPLICATION_LAYER_H_
#define _APPLICATION_LAYER_H_

#define FILE_NAME 1
#define FILE_SIZE 0

#define CONTROL_DATA 1
#define CONTROL_START 2
#define CONTROL_END 3

#define INVALID -1

typedef enum {
  READ_CONTROL_START,
  READ_FILESIZE,
  READ_FILENAME,
} READ_CONTROL_STATE;

typedef enum {
  READ_CONTROL_DATA,
  READ_DATA_L1,
  READ_DATA_L2,
  READ_DATA
} READ_FILE_STATE;

// Application layer main function.
// Arguments:
//   serialPort: Serial port name (e.g., /dev/ttyS0).
//   role: Application role {"tx", "rx"}.
//   baudrate: Baudrate of the serial port.
//   nTries: Maximum number of frame retries.
//   timeout: Frame timeout.
//   filename: Name of the file to send / receive.
void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename);

int transmitter_application_layer(const char *filename);
int receiver_application_layer();

int read_control_frame(int app_layer_control, int *file_size,
                       unsigned char *filename);
int read_file(int file_size, unsigned char *filename);

int send_file(FILE *file);

int send_control_frame(const char *filename, int file_size,
                       int app_layer_control);

#endif // _APPLICATION_LAYER_H_
