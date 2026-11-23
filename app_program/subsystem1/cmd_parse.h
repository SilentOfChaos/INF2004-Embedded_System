/*
 *  cmd_parse.h
 *  Buddy 1's parser for MQTT commands on inf2004/IS19/cmd (FR14, FR6,
 *  NFR14).  Each command is a short flat JSON record (report 6.1):
 *
 *    {"seq":12,"cmd":"remap","letter":"E","action":"left"}
 *    {"seq":13,"cmd":"default_map"}
 *    {"seq":14,"cmd":"summary"}
 *    {"seq":15,"cmd":"stop"}
 *    {"seq":16,"cmd":"broker","ip":"192.168.1.20","port":1883}
 *    {"seq":17,"cmd":"start"}   same as pressing START (GP20)
 *    {"seq":18,"cmd":"reset"}   same as pressing RESET (GP21)
 *
 *  Actions: none, straight, left, right, uturn.  Only flat string and
 *  integer values are understood; anything else is rejected.
 *
 *  Pure C, no kernel headers: tested on the host (testing/host/).
 */

#ifndef CMD_PARSE_H
#define CMD_PARSE_H

#include <stdbool.h>
#include <stdint.h>

#define CMD_IP_MAX      16u     /* "255.255.255.255" + NUL */

typedef struct
{
    uint8_t  code;              /* CMD_CODE_xxx (tlm_msgs.h) */
    uint32_t seq;               /* the sender's "seq", echoed in the ack */
    char     letter;            /* remap */
    uint8_t  action;            /* remap: letter_action_t */
    char     ip[CMD_IP_MAX];    /* broker */
    uint16_t port;              /* broker */
} cmd_t;

/*
 * An mbf_mqtt_cfg record (IF-15, Command -> MQTT code): the broker to use
 * from now on, packed into two words (minimum-bit change, D-91):
 *   word 0  bits  0-8   part 0   the address's four parts, each spelt
 *           bits  9-17  part 1   exactly as given (cmd_ip_valid(): 1 to
 *           bits 18-26  part 2   3 digits, at most 255), 0..365:
 *                                  "0".."9"     0..9
 *                                  "00".."99"   10 + value
 *                                  "000".."255" 110 + value
 *           bits 27-31  0 (spare)
 *   word 1  bits  0-8   part 3
 *           bits  9-15  0 (spare)
 *           bits 16-31  port     1..65535
 * The spelling is kept, not only the 32-bit address, because the MQTT
 * code hands the text to lwIP, which reads it with C's rules (a leading
 * 0 means octal): the same characters give the same broker.
 */
#define CMD_BROKER_WORDS    2u

typedef struct
{
    uint32_t word[CMD_BROKER_WORDS];
} comms_broker_t;

bool     comms_broker_pack(comms_broker_t * p_broker, char const * p_ip,
                           uint16_t port);
void     comms_broker_ip(comms_broker_t const * p_broker, char * p_ip,
                         uint32_t size);
uint16_t comms_broker_port(comms_broker_t const * p_broker);

bool cmd_parse(char const * p_text, uint32_t len, cmd_t * p_cmd);
bool cmd_is_stop(char const * p_text, uint32_t len);
bool cmd_json_string(char const * p_text, uint32_t len, char const * p_key,
                     char * p_out, uint32_t out_size);
bool cmd_json_int(char const * p_text, uint32_t len, char const * p_key,
                  int32_t * p_out);
bool cmd_ip_valid(char const * p_ip);

#endif /* CMD_PARSE_H */

/*** end of file ***/
