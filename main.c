#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>

#include "debug.h"
#include "lora_rfm95.h"
#include "supervisor.h"
#include "os_abstraction.h"

#define MAX_SLAVE 10

typedef enum {
    SLAVE_OFFLINE = 0,
    SLAVE_ONLINE,
    SLAVE_IDLE = SLAVE_ONLINE,
    SLAVE_BUSY,
    SLAVE_STATE_MAX
} slave_state_t;

struct slave {
    //int addr;
    int64_t last_status_time_ms;
    int is_online;
    struct messageFormat order;
    int64_t last_order_time_ms;
    bool ack_recieved;
    uint16_t last_requestID;
    slave_state_t state;
};

union splitData {
   uint8_t  totalFrames;
   uint8_t  FrameNumber;
   uint16_t packetID;
};

union splitSignature {
   uint8_t  signbyte[2];
   uint16_t signature;
};

static struct slave s_slaves[MAX_SLAVE] = {
    [0] = {.last_status_time_ms = 0,},
};

static pthread_mutex_t s_lora_tx_lock;
static pthread_t s_lora_thread;
static int s_thread_stop = 0;
static void *s_lora_thread_func(void *arg);
static void int_handler(int dummy);
void check_slave_status();
void sendNewOrder(uint8_t deviceID, PACKET_TYPE pktTyp);
void resendOldOrder(struct messageFormat *txData, uint8_t deviceID);
void clearOrder(uint8_t deviceID);
void sendSyncPacket();
void process_master_send(void);

//static uint16_t last_requestID = 0;
int  idx = 0;

struct messageFormat txData;
int64_t last_sync_sent_time_ms;

int main(int argc, char **argv)
{
    int ret;
    void *tmp;
    char choice = 0;
    int i;
    struct slave *slv = NULL;

    s_thread_stop = 0;
    for(i = 0; i < MAX_SLAVE; ++i){
      memset(s_slaves, 0, sizeof(s_slaves));
      //s_slaves[i].addr = i;
    }

    if (pthread_mutex_init(&s_lora_tx_lock, NULL) != 0)
    {
        print_err("mutex init has failed");
        return 1;
    }

    ret = pthread_create(&s_lora_thread, NULL, s_lora_thread_func, NULL);
    if (0 != ret) {
        print_err("Create lora thread failed");
        return -1;
    }
    signal(SIGINT, int_handler);

    print_inf("MAIN thread started");

    // Print menu
    do {
        printf("MENU:\n");
        printf("s. show slaves' status\n");
        printf("o. send order to specific slave\n");
        printf("q. QUIT\n");
        do {
            choice = getchar();
        } while ((choice == '\r') || (choice == '\n'));

        switch (choice) {
            case 's':
            for (i = 0; i < MAX_SLAVE; ++i) {
                printf("Slave #%d is %s\n", i, s_slaves[i].is_online ? "ONLINE" : "OFFLINE");
            }
            break;

            case 'o':
            printf("=== Send order:\n");
            printf(" Choose slave id [0-%d]: ", MAX_SLAVE - 1);
            scanf("%d", &ret);
            if ((ret < 0) || (ret >= MAX_SLAVE)) {
                printf("ERR: slave id %d is invalid", ret);
            } else {
                uint8_t address = ret;
                PACKET_TYPE pktTyp;
                slv = &s_slaves[ret];
                // Check if the slave is online
                if (!slv->is_online) {
                    // not online --> error
                    printf("ERR: slave with id %d is not ONLINE\n", ret);
                } else {
                    slv->order.destAddress = ret;
                    printf(" Choose order type:\n");
                    printf(" 0. Read analog sensor\n");
                    printf(" 1. Read digital sensor\n");
                    printf(" 2. Write GPIO output\n");
                    printf(" 3. Read GPIO input\n");
                    printf(" 4. Read I2C data\n");
                    printf(" 5. Read UART1 data\n");
                    printf(" 6. Read UART2 data\n");
                    scanf("%d", &ret);
                    if ((ret < 0) || (ret > 6)) {
                        printf("ERR: invalid order message type %d", ret);
                    } else {
                        sendNewOrder(address, REQ_READ_ANALOG_SENSOR + ret);
                    }
                }
            }

            break;

            case 'q':
            s_thread_stop = 1;
            break;

            default:
                printf("Invalid choice %c. Try again.\n", choice);
            break;
        }
    } while (!s_thread_stop);

    // Join the lora thread before quit
    ret = pthread_join(s_lora_thread, &tmp);
    print_inf("MAIN thread FINISHED");

    return 0;
}

static void *s_lora_thread_func(void *arg)
{
    int count = 0, len = 0;
    struct messageFormat msg;
    union splitSignature sign;
    char *msgPtr = (char *) &msg;
    char data;
    int not_interested = 0;
    int64_t time_ms;
    int i;
    struct timespec ts;

    count = loraInit();
    if (0 != count) {
        print_err("lora init failed");
        return NULL;
    }
    print_inf("loraInit done");

    while (!loraBegin(LORA_FREQUENCY)) {
        print_inf(".");
        os_delay_ms(1000);
        if (s_thread_stop) {
            goto lb_lora_exit;
        }
    }
    print_inf("Lora begin done");

    setSyncWord(0xF3);
    dumpRegisters();
    print_inf("dump register done!!!");
    // enable CRC check
    enableCrc();

    print_inf("LORA ready!!!");

    sign.signature = SSLA_SIGNATURE;

    while(!s_thread_stop) {
        // Receive packet
        pthread_mutex_lock(&s_lora_tx_lock);
        count = parsePacket(0);
        if (count) {
            not_interested = 0;
            memset(&msg, 0, sizeof(msg));
            print_dbg("Has message %dbytes with RSSI %d", count, packetRssi());
            count = 0;
            while (loraAvailable()) {
                data = loraRead();
                if(count < LORA_TX_BUFF_SIZE){
                    msgPtr[count] = data;
                }
                if(count == 3)
                {
                    if ( msgPtr[0] > MAX_SLAVE || msgPtr[1] != MASTER_DEVICE_ID || msgPtr[2] != sign.signbyte[0] || msgPtr[3] != sign.signbyte[1]) {
                        print_inf("Ignore message. id 0x%x 0x%x, sign 0x%x 0x%x sign_low = 0x%x sign_high = 0x%x",msgPtr[0], msgPtr[1], msgPtr[2], msgPtr[3],sign.signbyte[0],sign.signbyte[1] );
                        loraSleep(); // Enter sleep mode to clear FIFO
                        os_delay_ms(2);
                        loraIdle();  // Back to standby mode
                        not_interested = 1;
                        break;
                    }
                }
                count++;
            }
            print_dbg("Packet received: %d/%d bytes and request id 0x%x", count, sizeof(msg) , msg.requestID);
            if(!not_interested) {
                if (s_slaves[msg.srcAddress].last_requestID != msg.requestID ) {
                    s_slaves[msg.srcAddress].last_requestID = msg.requestID;
                    print_inf("RECV id %x, src %x, dst %x, sign %x, pkt id %x, type %d, len %d",
                        msg.requestID, msg.srcAddress, msg.destAddress, msg.signature, msg.packetID, msg.packetTyp, msg.length);
                    if ((msg.packetTyp >= RESP_DEVICE_STARTED) && (msg.packetTyp <= RESP_INVALID_FRAME_NUMBER_ID)) {
                        print_dbg("RESP packet received, slave #%d", msg.srcAddress);
                        switch (msg.packetTyp) {
                            case RESP_STATUS_PACKET:
                                // Status packet --> update slave status
                                if (!s_slaves[msg.srcAddress].is_online) {
                                    s_slaves[msg.srcAddress].is_online = 1;
                                    s_slaves[msg.srcAddress].state = SLAVE_ONLINE;
                                    print_inf("Slave #%d is ONLINE", msg.srcAddress);
                                }
                                clock_gettime(CLOCK_REALTIME, &ts);
                                s_slaves[msg.srcAddress].last_status_time_ms = (ts.tv_sec * 1000 + ts.tv_nsec / 10000000);
                                break;

                            case RESP_ACKNOWLEDGE_PACKET:
                            case RESP_ACKNOWLEDGE_INTERIM_PACKET:
                                if(s_slaves[msg.srcAddress].order.requestID == msg.requestID){
                                    s_slaves[msg.srcAddress].ack_recieved == true;
                                    print_inf("ACK from slave #%d was received", msg.srcAddress);
                                }
                                clearOrder(msg.srcAddress);
                            break;

                            case RESP_DEVICE_STARTED:
                            case RESP_INVALID_PACKET:
                            case RESP_INVALID_INTERIM_REQUEST_ID:
                            case RESP_INVALID_FRAME_NUMBER_ID:
                                // check_slave_status();
                            break;

                            case RESP_READ_ANALOG_SENSOR:
                            case RESP_READ_DIGITAL_SENSOR:
                            case RESP_WRITE_GPIO_OUTPUT_PINS:
                            case RESP_READ_GPIO_INPUT_PINS:
                            case RESP_READ_I2C_DATA:
                            case RESP_READ_UART1_DATA:
                            case RESP_READ_UART2_DATA:
                            //NICK recieved response to order packet
                              print_inf("RESP from slave #%d was received", msg.srcAddress);
                            break;

                            default:
                                printf("Invalid packetTyp.\n");
                        }
                    } else {
                        print_err("Received invalid packet %d, from slave #%d", msg.packetTyp, msg.srcAddress);
                    }
              } else {
                print_inf("message is same from 0x%x", msg.srcAddress);
              }
           } else {
                print_inf("not interested ");
           }
        }
        pthread_mutex_unlock(&s_lora_tx_lock);
        check_slave_status();
        process_master_send();
    }

lb_lora_exit:
    print_inf("LORA TASK DONE!!! QUIT NOW");
    return NULL;
}

static void int_handler(int dummy)
{
    signal(dummy, SIG_IGN);
    s_thread_stop = 1;
}

void check_slave_status()
{
  int64_t time_ms;
  int i;
  struct timespec ts;
  bool isSlaveOnline;
  os_delay_ms(5);
  clock_gettime(CLOCK_REALTIME, &ts);
  time_ms = ts.tv_sec * 1000 + ts.tv_nsec / 10000000;
  for (i = 0; i < MAX_SLAVE; i++) {
      if ((s_slaves[i].is_online) && ((time_ms - s_slaves[i].last_status_time_ms) >= 30000)) {
          // don't receive status message in 30s --> set offline
          s_slaves[i].is_online = 0;
          s_slaves[i].state = SLAVE_OFFLINE;
          print_inf("Slave #%d is OFFLINE", i);
      }
      //resend order since we did not recieve any ACK
      if ((s_slaves[i].is_online) && ((time_ms - s_slaves[i].last_order_time_ms) >= 5100) && s_slaves[i].order.requestID > 0 && s_slaves[i].ack_recieved == false) {
         resendOldOrder(&s_slaves[i].order ,i);
      }
  }

  isSlaveOnline = false;
  if((time_ms - last_sync_sent_time_ms) >= 3500 ) {
    for (i = 0; i < MAX_SLAVE; i++) {
       if (s_slaves[i].is_online){
         isSlaveOnline = true;
         break;
       }
    }
    if (!isSlaveOnline){
      //Send Sync packet
      sendSyncPacket();
    }
  }
}

void sendSyncPacket()
{
  struct messageFormat sync;
  struct timespec ts;
  uint16_t reqID = 0;

  pthread_mutex_lock(&s_lora_tx_lock);
  reqID = (rand() % 10000);
  sync.srcAddress  = (uint8_t) MASTER_DEVICE_ID;
  sync.destAddress = BROADCAST_SYNC_ID;
  sync.signature   = SSLA_SIGNATURE;
  sync.requestID   = reqID;
  sync.packetID    = 0x0101;
  sync.packetTyp   = REQ_SYNC_LAN_PACKET;
  sync.length      = 0;
  print_inf("BROADCAST SYNC to #%d, reqid %d, type %d", BROADCAST_SYNC_ID, sync.requestID, sync.packetTyp);

  beginPacket(0);
  loraWriteBuf((uint8_t *)&sync, LORA_HEADER_SIZE + sync.length);
  endPacket(0);
  pthread_mutex_unlock(&s_lora_tx_lock);
  clock_gettime(CLOCK_REALTIME, &ts);
  last_sync_sent_time_ms = (ts.tv_sec * 1000 + ts.tv_nsec / 10000000);
}

void sendNewOrder(uint8_t deviceID, PACKET_TYPE pktTyp)
{
  uint16_t reqID = 0;
  struct timespec ts;
  struct messageFormat *order = &s_slaves[deviceID].order;

  if ((!s_slaves[deviceID].is_online) || (s_slaves[deviceID].state == SLAVE_BUSY)) {
    print_err("Slave #%d isn't online or is busy", deviceID);
    return;
  }
  s_slaves[deviceID].state = SLAVE_BUSY;
  order->srcAddress  = (uint8_t) MASTER_DEVICE_ID;
  order->destAddress = deviceID;
  order->signature   = SSLA_SIGNATURE;
  order->requestID   = (rand() % 10000);
  order->packetID    = 0x0101;
  order->packetTyp   = pktTyp;
  order->length      = 0;
}

void resendOldOrder(struct messageFormat *txData, uint8_t deviceID)
{
  if (!s_slaves[deviceID].is_online) {
    return;
  }
  s_slaves[deviceID].state = SLAVE_BUSY;
}

void clearOrder(uint8_t deviceID) {
    memset(&s_slaves[deviceID].order, 0, sizeof(struct messageFormat));
    s_slaves[deviceID].state = SLAVE_ONLINE;
}

void process_master_send(void)
{
    int i;
    struct timespec ts;
    struct messageFormat *order = NULL;
    struct slave *slv = NULL;
    int64_t time_ms, curtime_ms;

    clock_gettime(CLOCK_REALTIME, &ts);
    curtime_ms = ts.tv_sec * 1000 + ts.tv_nsec / 10000000;
    time_ms = curtime_ms - last_sync_sent_time_ms;
    time_ms = time_ms % 5000; // precision of 5s

    if (time_ms < 4500) {
        // Not master slot
        return;
    }

    // Browse all slaves
    for (i = 0; i < MAX_SLAVE; ++i) {
        slv = &s_slaves[i];

        if ((!slv->is_online) || (slv->state != SLAVE_BUSY)) {
            // This slave isn't online or doesn't have message to transmit
            continue;
        }

        // Transmit order
        pthread_mutex_lock(&s_lora_tx_lock);
        order = &slv->order;
        print_inf("ORDER to #%d, reqid %d, type %d", order->destAddress, order->requestID, order->packetTyp);

        beginPacket(0);
        loraWriteBuf((uint8_t *)order, LORA_HEADER_SIZE + order->length);
        endPacket(0);
        os_delay_ms(1);
        pthread_mutex_unlock(&s_lora_tx_lock);
        slv->last_order_time_ms = curtime_ms;

        // Set state to IDLE
        slv->state = SLAVE_ONLINE;
    }

}
