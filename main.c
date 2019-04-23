#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>

#include "debug.h"
#include "lora.h"
#include "supervisor.h"
#include "os_abstraction.h"

#define MAX_SLAVE 30

typedef enum {
    SLAVE_OFFLINE = 0,
    SLAVE_ONLINE,
    SLAVE_IDLE = SLAVE_ONLINE,
    SLAVE_BUSY,
    SLAVE_STATE_MAX
} slave_state_t;

struct slave {
    int addr;
    int64_t last_status_time_ms;
    int is_online;
    struct messageFormat order;
    bool ack_recieved;
    slave_state_t state;
};

union splitData {
   uint8_t  totalFrames;
   uint8_t  FrameNumber;
   uint16_t packetID;
};

union splitSignature {
   uint8_t  sign_high;
   uint8_t  sign_low;
   uint16_t signature;
};


static struct slave s_slaves[MAX_SLAVE] = {
    [0] = {.addr = 0,},
    [1] = {.addr = 1,},
    [2] = {.addr = 2,},
    [3] = {.addr = 3,},
    [4] = {.addr = 4,},
};

static pthread_t s_lora_thread;
static int s_thread_stop = 0;
static void *s_lora_thread_func(void *arg);
static void int_handler(int dummy);

static uint16_t last_requestID = 0;
int  idx = 0;

struct messageFormat txData;

int main(int argc, char **argv)
{
    int ret;
    void *tmp;
    char choice = 0;
    int i;

    s_thread_stop = 0;
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
        printf("0. show slaves' status\n");
        printf("1. send order to slave 1\n");
        printf("2. send order to slave 2\n");
        printf("3. send order to slave 3\n");
        printf("4. send order to slave 4\n");
        printf("5. send order to slave 5\n");
        printf("q. QUIT\n");
        do {
            choice = getchar();
        } while ((choice == '\r') || (choice == '\n'));

        switch (choice) {
            case '0':
            for (i = 0; i < MAX_SLAVE; ++i) {
                printf("Slave #%d is %s", i, s_slaves[i].is_online ? "ONLINE" : "OFFLINE");
            }
            break;

            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
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
    int not_interested = 0;
    struct timespec ts;
    int64_t time_ms;
    int i;

    count = loraInit();
    if (0 != count) {
        print_err("lora init failed");
        return -1;
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
    dumpRegisters();

    setSyncWord(0xF3);
    dumpRegisters();
    print_inf("dump register done!!!");
    // enable CRC check
    enableCrc();

    print_inf("LORA ready!!!");

    sign.signature = SSLA_SIGNATURE;

    while(!s_thread_stop) {

        // Receive packet
        count = parsePacket(0);
        if (count) {
            not_interested = 0;
            memset(&msg, 0, sizeof(msg));
            print_dbg("Has message %dbytes with RSSI %d", count, packetRssi());
            count = 0;
            while (loraAvailable()) {
                msgPtr[count] = loraRead();
                if(count == 3)
                {
                    if (msgPtr[1] != MASTER_DEVICE_ID && msgPtr[2] != sign.sign_high && msgPtr[3] != sign.sign_low) {
                        print_inf("Ignore message. id %x, sign %x %x", msgPtr[1], msgPtr[2], msgPtr[3]);
                        loraSleep(); // Enter sleep mode to clear FIFO
                        os_delay_ms(2);
                        loraIdle();  // Back to standby mode
                        not_interested = 1;
                        break;
                    }
                }
                count++;
            }
            print_dbg("Packet received: %d/%d bytes", count, sizeof(msg));
            if(last_requestID != msg.requestID && !not_interested) {
                last_requestID = msg.requestID;
                print_dbg("RECV id %x, src %x, dst %x, sign %x, pkt id %x, type %d, len %d",
                    msg.requestID, msg.srcAddress, msg.destAddress, msg.signature, msg.packetID, msg.packetTyp, msg.length);
                if ((msg.packetTyp >= RESP_DEVICE_STARTED) && (msg.packetTyp <= RESP_INVALID_FRAME_NUMBER_ID)) {
                    print_inf("RESP packet received, slave #%d", msg.srcAddress);
                    switch (msg.packetTyp) {
                        case RESP_STATUS_PACKET:
                            // Status packet --> update slave status
                            if (!s_slaves[msg.srcAddress].is_online) {
                                s_slaves[msg.srcAddress].is_online = 1;
                                s_slaves[msg.srcAddress].state = SLAVE_ONLINE;
                                print_inf("Slave #%d is ONLINE", msg.srcAddress);
                            }
                            clock_gettime(CLOCK_REALTIME, &ts);
                            s_slaves[msg.srcAddress].last_status_time_ms = ts.tv_sec * 1000 + ts.tv_nsec / 10000000;
                        case RESP_ACKNOWLEDGE_PACKET:
                        case RESP_ACKNOWLEDGE_INTERIM_PACKET:
                            //NICK recieved ack for our order packet no need to resend last order
                            if(s_slaves[msg.srcAddress].order.requestID == msg.requestID){
                                 s_slaves[msg.srcAddress].ack_recieved == true;
                            }

                        break;
                        case RESP_DEVICE_STARTED:
                        case RESP_INVALID_PACKET:
                        case RESP_INVALID_INTERIM_REQUEST_ID:
                        case RESP_INVALID_FRAME_NUMBER_ID:
                        //NICK resend the last order again as slave thinks last order was incorrect or slave was rebooted.
                            check_slave_status();
                        break;

                        case RESP_READ_ANALOG_SENSOR:
                        case RESP_READ_DIGITAL_SENSOR:
                        case RESP_WRITE_GPIO_OUTPUT_PINS:
                        case RESP_READ_GPIO_INPUT_PINS:
                        case RESP_READ_I2C_DATA:
                        case RESP_READ_UART1_DATA:
                        case RESP_READ_UART2_DATA:
                        //NICK recieved response to order packet
                        break;

                        default:
                            printf("Invalid choice %c. Try again.\n", choice);
                            //NICK if you had sent an order last time to the slave and waiting for ACK
                            //resend the order as we did not recieve any ACK
                    }
                } else {
                    print_err("Received invalid packet %d, slave #%d", msg.packetTyp, msg.srcAddress);
                }
           } else {
                //NICK fix slave device because it is sending last packet again and again
           }
             check_slave_status();
        } else {
             check_slave_status();
        }
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
  os_delay_ms(5);
  clock_gettime(CLOCK_REALTIME, &ts);
  time_ms = ts.tv_sec * 1000 + ts.tv_nsec / 10000000;
  for (i = 0; i < MAX_SLAVE; ++i) {
      if ((s_slaves[i].is_online) && ((time_ms - s_slaves[i].last_status_time_ms) >= 10000)) {
          // don't receive status message in 10s --> set offline
          s_slaves[i].is_online = 0;
          s_slaves[i].state = SLAVE_OFFLINE;
          print_inf("Slave #%d is OFFLINE", i);
      }
  }
  //resend order since we did not recieve any ACK
  for (i = 0; i < MAX_SLAVE; ++i) {
      if ((s_slaves[i].is_online) && ((time_ms - s_slaves[i].last_status_time_ms) >= 1000) && s_slaves[i].order.requestID > 0 && s_slaves[i].ack_recieved == false) {
          //resend last order as we did not recieve ACK
      }
  }
}
