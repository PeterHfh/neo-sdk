#include <stdio.h>
#include <stdlib.h>

#include "protocol.h"

namespace neo {
namespace protocol {

const uint8_t DATA_ACQUISITION_START[2] = {'D', 'S'};
const uint8_t DATA_ACQUISITION_STOP[2] = {'D', 'X'};
const uint8_t MOTOR_SPEED_ADJUST[2] = {'M', 'S'};
const uint8_t MOTOR_INFORMATION[2] = {'M', 'I'};
const uint8_t SAMPLE_RATE_ADJUST[2] = {'L', 'R'};
const uint8_t SAMPLE_RATE_INFORMATION[2] = {'L', 'I'};
const uint8_t VERSION_INFORMATION[2] = {'I', 'V'};
const uint8_t DEVICE_INFORMATION[2] = {'I', 'D'};
const uint8_t RESET_DEVICE[2] = {'R', 'R'};
const uint8_t DEVICE_CALIBRATION[2] = {'C', 'S'};

typedef struct error {
  const char* what; // always literal, do not deallocate
} error;

// Constructor hidden from users
static error_s error_construct(const char* what) {
  NEO_ASSERT(what);

  auto out = new error{what};
  return out;
}

const char* error_message(error_s error) {
  NEO_ASSERT(error);

  return error->what;
}

void error_destruct(error_s error) {
    //if(error == nullptr)return;
  NEO_ASSERT(error);
    //printf("error :::: %s\n",neo::protocol::error_message(error));
    delete error;
    //error = nullptr;
}

static uint8_t checksum_response_header(response_header_s* v) {
  NEO_ASSERT(v);

  return ((v->cmdStatusByte1 + v->cmdStatusByte2) & 0x3F) + 0x30;
}

static uint8_t checksum_response_param(response_param_s* v) {
  NEO_ASSERT(v);

  return ((v->cmdStatusByte1 + v->cmdStatusByte2) & 0x3F) + 0x30;
}

static uint8_t checksum_response_scan_packet(response_scan_packet_s* v) {
  NEO_ASSERT(v);

  uint64_t checksum = 0;
  checksum += (v->distance_low << 3) + (v->VHL << 2) + (v->s2 << 1) + v->s1;
  checksum += v->distance_high;
  checksum += v->angle & 0x00ff;
  checksum += v->angle >> 8;
  checksum += v->VRECT << 4;
  return checksum % 15;
}

void write_command(serial::device_s serial, const uint8_t cmd[2], error_s* error) {
  NEO_ASSERT(serial);
  NEO_ASSERT(cmd);
  NEO_ASSERT(error);

  cmd_packet_s packet;
  packet.cmdByte1 = cmd[0];
  packet.cmdByte2 = cmd[1];
  packet.cmdParamTerm = '\n';

  serial::error_s serialerror = nullptr;

  serial::device_write(serial, &packet, sizeof(cmd_packet_s), &serialerror);

  if (serialerror) {
    *error = error_construct("unable to write command");
    serial::error_destruct(serialerror);
    return;
  }
}

void write_command_with_arguments(serial::device_s serial, const uint8_t cmd[2], const uint8_t arg[2], error_s* error) {
  NEO_ASSERT(serial);
  NEO_ASSERT(cmd);
  NEO_ASSERT(arg);
  NEO_ASSERT(error);

  cmd_param_packet_s packet;
  packet.cmdByte1 = cmd[0];
  packet.cmdByte2 = cmd[1];
  packet.cmdParamByte1 = arg[0];
  packet.cmdParamByte2 = arg[1];
  packet.cmdParamTerm = '\n';

  serial::error_s serialerror = nullptr;

  serial::device_write(serial, &packet, sizeof(cmd_param_packet_s), &serialerror);

  if (serialerror) {
    *error = error_construct("unable to write command with arguments");
    serial::error_destruct(serialerror);
    return;
  }
}

void read_response_header(serial::device_s serial, const uint8_t cmd[2], response_header_s* header, error_s* error) {
  NEO_ASSERT(serial);
  NEO_ASSERT(cmd);
  NEO_ASSERT(header);
  NEO_ASSERT(error);

  serial::error_s serialerror = nullptr;

  serial::device_read(serial, header, sizeof(response_header_s), &serialerror);

  if (serialerror) {
    *error = error_construct("unable to read response header");
    serial::error_destruct(serialerror);
    return;
  }

  uint8_t checksum = checksum_response_header(header);

  if (checksum != header->cmdSum) {
    *error = error_construct("invalid response header checksum");
    return;
  }

  bool ok = header->cmdByte1 == cmd[0] && header->cmdByte2 == cmd[1];

  if (!ok) {
    *error = error_construct("invalid header response commands");
    return;
  }
}

void read_response_param(serial::device_s serial, const uint8_t cmd[2], response_param_s* param, error_s* error) {
  NEO_ASSERT(serial);
  NEO_ASSERT(cmd);
  NEO_ASSERT(param);
  NEO_ASSERT(error);

  serial::error_s serialerror = nullptr;

  serial::device_read(serial, param, sizeof(response_param_s), &serialerror);

  if (serialerror) {
    *error = error_construct("unable to read response param header");
    serial::error_destruct(serialerror);
    return;
  }

  uint8_t checksum = checksum_response_param(param);

  if (checksum != param->cmdSum) {
    *error = error_construct("invalid response param header checksum");
    return;
  }

  bool ok = param->cmdByte1 == cmd[0] && param->cmdByte2 == cmd[1];

  if (!ok) {
    *error = error_construct("invalid param response commands");
    return;
  }
}

void read_response_scan(serial::device_s serial, response_scan_packet_s* scan, error_s* error) {
  NEO_ASSERT(serial);
  NEO_ASSERT(scan);
  NEO_ASSERT(error);

  serial::error_s serialerror = nullptr;

  serial::device_read(serial, scan, sizeof(response_scan_packet_s), &serialerror);
  if (serialerror) {
    *error = error_construct("invalid response scan packet checksum");
    serial::error_destruct(serialerror);
    return;
  }
  uint8_t checksum = checksum_response_scan_packet(scan);
  char *p = (char*)scan;
  char count = 100; // error count.
  while (checksum != scan->checksum && count > 0 ) {
  /*    if(1)
      {
          for (int i = 0; i < 5; ++i) {
              printf("%02X ",(unsigned char)p[i]);
          }
          printf("\n");
      }*/
    p[0] = p[1];
    p[1] = p[2];
    p[2] = p[3];
    p[3] = p[4];
    serial::device_read(serial, &(p[4]), 1, &serialerror);
      //printf("11112222\n");
    if (serialerror) {
        //printf("3333222\n");
        *error = error_construct("invalid response scan packet checksum");
        serial::error_destruct(serialerror);
        return;
    }
    checksum = checksum_response_scan_packet(scan);
    if((p[0]&0x03) != 0x00)checksum = 0x00;
    count--;

    //printf("error checksum count:  %d \n",count);
  }
  /*  if(count < 10 && count > 7)
    {
        for (int i = 0; i < 5; ++i) {
            printf("%02X ",(unsigned char)p[i]);
        }
        printf("\n");
    }*/
    //printf("2222\n");
  if (checksum != scan->checksum) {
    *error = error_construct("invalid scan response commands");
    return;
  }

}

void read_response_info_motor(serial::device_s serial, const uint8_t cmd[2], response_info_motor_s* info, error_s* error) {
  NEO_ASSERT(serial);
  NEO_ASSERT(cmd);
  NEO_ASSERT(info);
  NEO_ASSERT(error);

  serial::error_s serialerror = nullptr;

  serial::device_read(serial, info, sizeof(response_info_motor_s), &serialerror);

  if (serialerror) {
    *error = error_construct("unable to read response motor info");
    serial::error_destruct(serialerror);
    return;
  }

  bool ok = info->cmdByte1 == cmd[0] && info->cmdByte2 == cmd[1];

  if (!ok) {
    *error = error_construct("invalid motor info response commands");
    return;
  }
}

void read_response_info_sample_rate(neo::serial::device_s serial, const uint8_t cmd[2], response_info_sample_rate_s* info,
                                    error_s* error) {
  NEO_ASSERT(serial);
  NEO_ASSERT(cmd);
  NEO_ASSERT(info);
  NEO_ASSERT(error);

  serial::error_s serialerror = nullptr;

  serial::device_read(serial, info, sizeof(response_info_sample_rate_s), &serialerror);

  if (serialerror) {
    *error = error_construct("unable to read response sample rate info");
    serial::error_destruct(serialerror);
    return;
  }

  bool ok = info->cmdByte1 == cmd[0] && info->cmdByte2 == cmd[1];

  if (!ok) {
    *error = error_construct("invalid sample rate info response commands");
    return;
  }
}

} // namespace protocol
} // namespace neo
