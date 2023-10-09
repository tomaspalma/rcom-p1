// Link layer protocol implementation
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "link_layer.h"
#include "serial_port.h"

// MISC

int fd;
bool resend = FALSE;
int send_tries = 0;

int ns = 0;
int nr = 1;

enum OPEN_STATES { START_RCV, FLAG_RCV, A_RCV, C_RCV, BCC_RCV };

enum READ_STATES { START, READ_FLAG, READ_A, READ_C, READ_DATA, ESC_RCV};

void resend_timer(int signal) {
  resend = TRUE;
  send_tries++;
  alarm(TIMEOUT);
}

int send_supervised_set() {
  unsigned char set[SETUP_COMM_MSG_SIZE] = {DELIMETER, A_SENDER, C_SET,
                                            A_SENDER ^ C_SET, DELIMETER};

  // TODO Refactor this so we exit after alarm tries reach limit
  if (write(fd, set, SETUP_COMM_MSG_SIZE) == -1) {
    printf("Failed writing SET message to serial port\n");
    return -1;
  }

  return 0;
}

void recv_set_state_machine() {
  enum OPEN_STATES set_state = START_RCV;
  unsigned char recv_set = 0;
  unsigned char stop = FALSE;
  while (stop == FALSE) {
    read(fd, &recv_set, 1);

    printf("Received: %x\n", recv_set);

    if (set_state == START_RCV) {
      if (recv_set == DELIMETER)
        set_state = FLAG_RCV;
      else
        set_state = START_RCV;
    } else if (set_state == FLAG_RCV) {
      if (recv_set == DELIMETER)
        set_state = FLAG_RCV;
      else if (recv_set == A_SENDER)
        set_state = A_RCV;
      else
        set_state = START_RCV;
    } else if (set_state == A_RCV) {
      if (recv_set == DELIMETER)
        set_state = FLAG_RCV;
      else if (recv_set == C_SET)
        set_state = C_RCV;
      else
        set_state = START_RCV;
    } else if (set_state == C_RCV) {
      if (recv_set == DELIMETER)
        set_state = FLAG_RCV;
      else if (recv_set == (A_SENDER ^ C_SET))
        set_state = BCC_RCV;
      else
        set_state = START_RCV;
    } else if (set_state == BCC_RCV) {
      if (recv_set == DELIMETER)
        stop = TRUE;
      else
        set_state = START_RCV;
    }
  }
}

void recv_ua_state_machine() {
  enum OPEN_STATES ua_state = START_RCV;
  unsigned char recv_ua = 0;
  unsigned char stop = FALSE;
  while (stop == FALSE) {
    if (resend) {
      if (send_tries >= 3) {
        return;
      }

      ua_state = START_RCV;
      resend = false;
      if (send_supervised_set() != 0)
        return;
    }

    read(fd, &recv_ua, 1);

    printf("Received: %x\n", recv_ua);

    if (ua_state == START_RCV) {
      if (recv_ua == DELIMETER)
        ua_state = FLAG_RCV;
      else
        ua_state = START_RCV;
    } else if (ua_state == FLAG_RCV) {
      if (recv_ua == DELIMETER)
        ua_state = FLAG_RCV;
      else if (recv_ua == A_RECEIVER)
        ua_state = A_RCV;
      else
        ua_state = START_RCV;
    } else if (ua_state == A_RCV) {
      if (recv_ua == DELIMETER)
        ua_state = FLAG_RCV;
      else if (recv_ua == C_UA)
        ua_state = C_RCV;
      else
        ua_state = START_RCV;
    } else if (ua_state == C_RCV) {
      if (recv_ua == DELIMETER)
        ua_state = FLAG_RCV;
      else if (recv_ua == (A_RECEIVER ^ C_UA))
        ua_state = BCC_RCV;
      else
        ua_state = START_RCV;
    } else if (ua_state == BCC_RCV) {
      if (recv_ua == DELIMETER) {
        alarm(0);
        send_tries = 0;
        stop = TRUE;
      } else
        ua_state = START_RCV;
    }
  }
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters) {
  fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);
  if (fd == -1)
    return -1;

  setup_port(fd);

  if (connectionParameters.role == LlRx) {
    recv_set_state_machine();

    unsigned char ua[SETUP_COMM_MSG_SIZE] = {DELIMETER, A_RECEIVER, C_UA,
                                             A_RECEIVER ^ C_UA, DELIMETER};
    write(fd, ua, SETUP_COMM_MSG_SIZE);
  } else if (connectionParameters.role == LlTx) {
    (void)signal(SIGALRM, resend_timer);

    alarm(connectionParameters.timeout);
    if (send_supervised_set() != 0)
      return -1;
    recv_ua_state_machine();
  } else {
    return -1;
  }

  return 1;
}

int assemble_start_frame(unsigned char *frame) {
  if (frame == NULL) {
    return -1;
  }

  frame[0] = DELIMETER;
  frame[1] = A_SENDER;
  frame[2] = (1 << 6) | ns;
  frame[3] = frame[1] ^ frame[2];

  return 0;
}

int assemble_end_frame(int offset, unsigned char *frame, unsigned char bcc2) {
  if (frame == NULL) {
    return -1;
  }

  frame[offset] = bcc2;
  frame[offset + 1] = DELIMETER;

  return 0;
}

int stop_and_wait(unsigned char *frame, int size) {
  alarm(TIMEOUT);
  int ntries = 0;
  bool received = FALSE;
  resend = FALSE;

  if (write(fd, frame, size) == -1) {
    printf("Error writing frame in the link layer!");
    return -1;
  }

  enum OPEN_STATES current_state = START_RCV;
  unsigned char buf;
  while (!received) {
    if (resend) {
      if (send_tries >= N_TRIES) {
        return -1;
      }

      current_state = START_RCV;
      resend = false;
      if (write(fd, frame, size) == -1) {
        printf("Error writing frame in the link layer!");
        return -1;
      }
    }

    read(fd, &buf, 1);
    unsigned char confirmation_byte = 0;

    if (current_state == START_RCV) {
      if (buf == DELIMETER)
        current_state = FLAG_RCV;
    } else if (current_state == FLAG_RCV) {
      if (buf == DELIMETER)
        current_state = FLAG_RCV;
      else if (buf == A_RECEIVER)
        current_state = A_RCV;
      else
        current_state = START_RCV;
    } else if (current_state == A_RCV) {
      if (buf == DELIMETER)
        current_state = FLAG_RCV;
      else if (buf == C_RR(nr))
        current_state = C_RCV;
      else
        current_state = START_RCV;
    } else if (current_state == C_RCV) {
      if (buf == DELIMETER)
        current_state = FLAG_RCV;
      else if (buf == (A_RECEIVER ^ C_RR(nr)) ||
               buf == (A_RECEIVER ^ C_REJ(nr))) {
        confirmation_byte = buf;
        current_state = BCC_RCV;
      } else
        current_state = START_RCV;
    } else if (current_state == BCC_RCV) {
      if (buf == DELIMETER) {
        alarm(0);
        send_tries = 0;

        if (confirmation_byte == C_RR(1) || confirmation_byte == C_RR(0)) {
          resend = TRUE;
          received = TRUE;
          ns = (ns + 1) % 2;
        } else if (confirmation_byte == C_REJ(0) ||
                   confirmation_byte == C_REJ(1)) {
          resend = FALSE;
        }
      } else
        current_state = START_RCV;
    }
  }

  return 0;
}

/**
 * @brief
 *
 * @param buf Buffer of bytes of the data
 * @param bufSize Number of bytes of data
 * @return int
 */
int llwrite(const unsigned char *buf, int bufSize) {
  unsigned char *frame = (unsigned char *)malloc(HEADER_START_SIZE +
                                                 bufSize * 2 + HEADER_END_SIZE);
  assemble_start_frame(frame);

  unsigned char bcc2 = 0;
  int byte_stuffing_offset = 0;
  int frame_index = HEADER_START_SIZE;
  for (int i = 0; i < bufSize; i++) {
    if (buf[i + byte_stuffing_offset] == DELIMETER) {
      frame[frame_index] = ESCAPE;
      frame[frame_index + 1] = ESCAPED_DELIMITER;
      bcc2 ^= (ESCAPE ^ ESCAPED_DELIMITER);
      byte_stuffing_offset++;

      continue;
    }

    bcc2 ^= buf[i + byte_stuffing_offset];

    frame[frame_index] = buf[i + byte_stuffing_offset];
    frame_index += (1 + byte_stuffing_offset);
  }

  assemble_end_frame(bufSize + HEADER_START_SIZE, frame, bcc2);

  int number_of_wrote_bytes =
      bufSize + byte_stuffing_offset + HEADER_START_SIZE + HEADER_END_SIZE;

  if (stop_and_wait(frame, number_of_wrote_bytes) != 0) {
  }

  printf("\n");

  return number_of_wrote_bytes;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet) {
  unsigned char byte = 0;
  unsigned char frame_number = 2;
  enum READ_STATES read_state = START;
  unsigned char stop = FALSE;
  int i = 0;
  while (!stop) {
    read(fd, &byte, 1);

    if (read_state == START) {
      if (byte == DELIMETER)
        read_state = READ_FLAG;
      else
        read_state = START;
    }
    else if (read_state == READ_FLAG) {
      if (byte == A_SENDER)
        read_state = READ_A;
      else
        read_state = START;
    }
    else if (read_state == READ_A) {
      if (byte == C_NUMBER0 || byte == C_NUMBER1) {
        read_state = READ_C;
        frame_number = byte;
      }
      else if (byte == DELIMETER)
        read_state = READ_FLAG;
      else
        read_state = START;
    }
    else if (read_state == READ_C) {
      if (byte == (A_SENDER ^ frame_number))
        read_state = READ_DATA;
      else if (byte == DELIMETER)
        read_state = READ_FLAG;
      else
        read_state = START;
    }
    else if (read_state == READ_DATA) {
      if (byte == ESCAPE)
        read_state = ESC_RCV;
      else if (byte == DELIMETER) {
        unsigned char recv_bcc2 = packet[i-1];
        packet[--i] = 0;

        unsigned char calc_bcc2 = packet[0];
        for (unsigned int j = 1; j < i; j++)
          calc_bcc2 ^= packet[j];
        
        if (recv_bcc2 == calc_bcc2) {
          stop = TRUE;
          unsigned char acceptance_frame[5] = { DELIMETER, A_RECEIVER, C_RR(nr), A_RECEIVER ^ C_RR(nr), DELIMETER };
          write(fd, acceptance_frame, 5);
          nr = (nr + 1) % 2;
          return i;
        }
        else {
          unsigned char rejection_frame[5] = { DELIMETER, A_RECEIVER, C_REJ(nr), A_RECEIVER & C_REJ(nr), DELIMETER };
          write(fd, rejection_frame, 5);
          return 0;
        }
      }
    }
    else if (read_state == ESC_RCV) {
      read_state = READ_DATA;
      if (byte == ESCAPED_DELIMITER)
        packet[i++] = DELIMETER;
      else if (byte == ESCAPED_ESCAPE)
        packet[i++] = ESCAPE;
      else {
        packet[i++] = ESCAPE;
        packet[i++] = byte;
      }
    }
  }

  return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics) {
  // TODO

  return 1;
}
