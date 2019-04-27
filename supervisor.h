/* Filename    : supervisor update header
   Description : configures, GPIO, handles other tasks
   Author      : http://www.ssla.co.uk

   This software is SSLA licensed
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#ifndef _SUPERVISOR_H_
#define _SUPERVISOR_H_

#include <stdint.h>

#define SUPERVISOR_STACK_SIZE                              8192
#define SUPERVISOR_TASK_PRIORITY                           5

#define OTA_STACK_SIZE                                     4096
#define OTA_TASK_PRIORITY                                  5

#define SOFTWARE_UPDATE_STACK_SIZE                         4096
#define SOFTWARE_UPDATE_TASK_PRIORITY                      5

#define TX_MESSGAGE_QUEUE_SIZE                             10
#define MASTER_DEVICE_ID                                   0x80
#define BROADCAST_SYNC_ID                                  0xF0
#define SSLA_SIGNATURE                                     0x4224
#define LORA_TX_BUFF_SIZE                                  250
#define LORA_HEADER_SIZE                                   13
#define MAX_REQUESTS                                       6
#define MAX_BUFF_PER_REQ                                   2048
//structure index that we ping pong between IoT's
#define SRC_ADDRESS                                        0
#define DEST_ADDRESS                                       SRC_ADDRESS + 1
#define SIGN_LOW_BYTE                                      DEST_ADDRESS + 1
#define SIGN_HIGH_BYTE                                     SIGN_LOW_BYTE + 1
#define REQUEST_ID_LOW                                     SIGN_HIGH_BYTE + 1
#define REQUEST_ID_HIGH                                    REQUEST_ID_LOW + 1
#define FRAME_NUMBER                                       REQUEST_ID_HIGH + 1
#define TOTAL_FRAMES                                       FRAME_NUMBER + 1
#define SSLA_PACKET_TYPE                                   TOTAL_FRAMES + 1
#define SSLA_PAYLOAD_LENGTH                                SSLA_PACKET_TYPE + 1
#define SSLA_PAYLOAD                                       SSLA_PAYLOAD_LENGTH + 1
#define PACKET_END_NO_FRAMES                               0x0101

extern unsigned short             db_lora_reg;
extern unsigned short             db_device_id;
extern unsigned short             db_signature;

typedef enum {
               /*this type of packetTyp is recieved by SLAVE*/
               REQ_SYNC_LAN_PACKET = 1,
               REQ_INTERIM_PACKET = 2,
               REQ_READ_ANALOG_SENSOR,
               REQ_READ_DIGITAL_SENSOR,
               REQ_WRITE_GPIO_OUTPUT_PINS,
               REQ_READ_GPIO_INPUT_PINS,
               REQ_READ_I2C_DATA,
               REQ_READ_UART1_DATA,
               REQ_READ_UART2_DATA,

               /*this type of packetTyp is recieved by MASTER*/
               RESP_DEVICE_STARTED = 116,
               RESP_STATUS_PACKET,
               RESP_ACKNOWLEDGE_PACKET,
               RESP_ACKNOWLEDGE_INTERIM_PACKET,
               RESP_READ_ANALOG_SENSOR,
               RESP_READ_DIGITAL_SENSOR,
               RESP_WRITE_GPIO_OUTPUT_PINS,
               RESP_READ_GPIO_INPUT_PINS,
               RESP_READ_I2C_DATA,
               RESP_READ_UART1_DATA,
               RESP_READ_UART2_DATA,
               RESP_INVALID_PACKET,
               RESP_INVALID_INTERIM_REQUEST_ID,
               RESP_INVALID_FRAME_NUMBER_ID         // currently supporting maximum from of 1
} PACKET_TYPE;

struct __attribute((__packed__)) messageFormat {
    uint8_t     srcAddress;                            // From address
    uint8_t     destAddress;                           // To address
    uint16_t    signature;                             // signature byte to identify the packet is from SSLA
    uint16_t    requestID;                             // request id represents the requests from master to slave
    uint16_t    packetID;                              // byte1 is total frames byte2 frame no
    uint8_t     packetTyp;                             // TAG specifies the operations to do
    uint8_t     length;                                // Just length of the payload
    uint8_t     payload[LORA_TX_BUFF_SIZE - 10];       // payload
};

struct __attribute((__packed__)) allRequests {
    PACKET_TYPE packetTyp;
    uint16_t    packetID;                                   // byte1 is total frames byte2 frame no
    uint16_t    requestID;                                  // request id represents the requests from master to slave
    uint8_t     lora_Rxmessage[MAX_BUFF_PER_REQ];           // payload
};

extern struct messageFormat txData;
extern struct allRequests   allReq[MAX_REQUESTS];

#endif
