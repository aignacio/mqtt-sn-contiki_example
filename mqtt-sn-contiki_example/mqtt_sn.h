/**
  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing,
  software distributed under the License is distributed on an
  "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
  KIND, either express or implied.  See the License for the
  specific language governing permissions and limitations
  under the License.

 *******************************************************************************
 * @license This project is licensed under APACHE 2.0.
 * @file mqtt_sn.h
 * @brief Header of prototypes and cfg variables for the MQTT-SN API
 * @author Ânderson Ignácio da Silva
 * @date 19 Ago 2016
 * @brief Main file for the MQTT-SN Contiki PORT
 * @see http://www.aignacio.com
 */

#ifndef MQTT__SN_H
#define MQTT__SN_H

#include "simple-udp.h"
#include "clock.h"
#include "etimer.h"
#include "ctimer.h"
#include "list.h"
#include "net/ip/uip-debug.h"
#include "sys/ctimer.h"
#include <stdbool.h>

/*! \addtogroup MQTT_SN_DEBUG
*  DEBUG Macros for MQTT-SN
*  @{
*/
/*!
  @brief If the below macro is defined, debug message will apear in the console window of the devices
*/
//#define DEBUG_MQTT_SN
/*!
  @brief  If defined, enables the OS messages (Contiki friendly) used by the MQTT-SN API
*/
#define DEBUG_OS
/*!
  @brief If defined, enables the debug messages of the tasks used in the process of initialize a MQTT-SN Connection
*/
#define DEBUG_TASK
//#define DEBUG_UDP
/** @}*/

#ifdef DEBUG_TASK
#define debug_task(fmt, args...) printf("\n[Tarefa] "fmt, ##args)
#else
#define debug_task(fmt, ...)
#endif

#ifdef DEBUG_OS
#define debug_os(fmt, args...) printf("\n[DEMO] "fmt, ##args)
#else
#define debug_os(fmt, ...)
#endif

#ifdef DEBUG_MQTT_SN
#define debug_mqtt(fmt, args...) printf("\n[MQTT-SN] "fmt, ##args)
#else
#define debug_mqtt(fmt, ...)
#endif

#ifdef DEBUG_UDP
#define debug_udp(fmt, args...) printf("\n[UDP] "fmt, ##args)
#else
#define debug_udp(fmt, ...)
#endif

/*! \addtogroup MQTT_SN_CONTROL
*  Protocol macros for MQTT-SN
*  @{
*/
#define MQTT_SN_MAX_PACKET_LENGTH  (255)
#define MQTT_SN_MAX_TOPIC_LENGTH   (MQTT_SN_MAX_PACKET_LENGTH-6)

#define MQTT_SN_TYPE_ADVERTISE     (0x00)
#define MQTT_SN_TYPE_SEARCHGW      (0x01)
#define MQTT_SN_TYPE_GWINFO        (0x02)
#define MQTT_SN_TYPE_CONNECT       (0x04)
#define MQTT_SN_TYPE_CONNACK       (0x05)
#define MQTT_SN_TYPE_WILLTOPICREQ  (0x06)
#define MQTT_SN_TYPE_WILLTOPIC     (0x07)
#define MQTT_SN_TYPE_WILLMSGREQ    (0x08)
#define MQTT_SN_TYPE_WILLMSG       (0x09)
#define MQTT_SN_TYPE_REGISTER      (0x0A)
#define MQTT_SN_TYPE_REGACK        (0x0B)
#define MQTT_SN_TYPE_PUBLISH       (0x0C)
#define MQTT_SN_TYPE_PUBACK        (0x0D)
#define MQTT_SN_TYPE_PUBCOMP       (0x0E)
#define MQTT_SN_TYPE_PUBREC        (0x0F)
#define MQTT_SN_TYPE_PUBREL        (0x10)
#define MQTT_SN_TYPE_SUBSCRIBE     (0x12)
#define MQTT_SN_TYPE_SUBACK        (0x13)
#define MQTT_SN_TYPE_UNSUBSCRIBE   (0x14)
#define MQTT_SN_TYPE_UNSUBACK      (0x15)
#define MQTT_SN_TYPE_PINGREQ       (0x16)
#define MQTT_SN_TYPE_PINGRESP      (0x17)
#define MQTT_SN_TYPE_DISCONNECT    (0x18)
#define MQTT_SN_TYPE_WILLTOPICUPD  (0x1A)
#define MQTT_SN_TYPE_WILLTOPICRESP (0x1B)
#define MQTT_SN_TYPE_WILLMSGUPD    (0x1C)
#define MQTT_SN_TYPE_WILLMSGRESP   (0x1D)
#define MQTT_SN_TYPE_SUB_WILDCARD  (0x1E)

#define MQTT_SN_TOPIC_TYPE_NORMAL     (0x00)
#define MQTT_SN_TOPIC_TYPE_PREDEFINED (0x01)
#define MQTT_SN_TOPIC_TYPE_SHORT      (0x02)

#define MQTT_SN_FLAG_DUP     (0x1 << 7)
#define MQTT_SN_FLAG_QOS_0   (0x0 << 5)
#define MQTT_SN_FLAG_QOS_1   (0x1 << 5)
#define MQTT_SN_FLAG_QOS_2   (0x2 << 5)
#define MQTT_SN_FLAG_QOS_N1  (0x3 << 5)
#define MQTT_SN_FLAG_RETAIN  (0x1 << 4)
#define MQTT_SN_FLAG_WILL    (0x1 << 3)
#define MQTT_SN_FLAG_CLEAN   (0x1 << 2)

#define MQTT_SN_PROTOCOL_ID  (0x01)

#define ACCEPTED                    0x00
#define REJECTED_CONGESTION         0x01
#define REJECTED_INVALID_TOPIC_ID   0x02
#define REJECTED_NOT_SUPPORTED      0x03

#define MQTT_SN_TOPIC_TYPE_NORMAL     (0x00)
#define MQTT_SN_TOPIC_TYPE_PREDEFINED (0x01)
#define MQTT_SN_TOPIC_TYPE_SHORT      (0x02)
/** @}*/

/*! \addtogroup MQTT_SN_CONTROL
*  Control macros for MQTT-Sn
*  @{
*/
#define ss(x) sizeof(x)/sizeof(*x)               /**< Compute the size of a vector of pointers, used by some functions */
#define MQTT_SN_AUTO_RECONNECT                   /**< Define if the device will try to reconnect or not to the MQTT-SN broker, if defined enables it */
#define MQTT_SN_RETRY_PING        5              /**< Number of attempts of send PING REQUEST beforce disconnect to the node <-> broker */
#define MQTT_SN_TIMEOUT_CONNECT   9*CLOCK_SECOND /**< Timeout of communication between MQTT-SN broker <-> node in the CONNECT STEP */
#define MQTT_SN_TIMEOUT           3*CLOCK_SECOND /**< Timeout of communication between MQTT-SN broker <-> node already connected */
#define MQTT_SN_RETRY             5              /**< Number of attemps of send any packet to the broker before disconnect */
#define MAX_QUEUE_MQTT_SN         100            /**< Max. number of tasks to be dynamic allocated in the MQTT-SN API */
#define MAX_TOPIC_USED            100            /**< Max. number of topics that the user can register,the API creates a set of structures for the bind between topic ID and short topic ID */
/** @}*/

/*! \addtogroup Pacotes
*  DEBUG macros used for the MQTT-SN protocol
*  @{
*/
/** @struct disconnect_packet_t
 *  @brief Structures of disconnect packet for the MQTT-SN
 *  @var disconnect_packet_t::length
 *    Length of the packet
 *  @var disconnect_packet_t::msg_type
 *    Type of message
 *  @var disconnect_packet_t::duration
 *    Time duration of disconnection, used for sleeping devices (see the MQTT-SNv1.2, broker specificaiton)
 */
typedef struct __attribute__((packed)){
  uint8_t length;
  uint8_t  msg_type;
  uint16_t duration;
} disconnect_packet_t;

/** @struct ping_req_t
 *  @brief Structures of ping packet for the MQTT-SN
 *  @var ping_req_t::length
 *    Length of the packet
 *  @var ping_req_t::msg_type
 *    Type of message
 *  @var ping_req_t::client_id
 *    Client ID for the MQTT-SN Connection
 */
typedef struct __attribute__((packed)){
  uint8_t length;
  uint8_t  msg_type;
  char client_id[23];
} ping_req_t;

/** @struct publish_packet_t
 *  @brief Publish structure packet for the MQTT-SN
 *  @var publish_packet_t::length
 *    Length of the packet
 *  @var publish_packet_t::type
 *    Type of message
 *  @var publish_packet_t::flags
 *    Flags used (retain,DUP,QoS...)
 *  @var publish_packet_t::topic_id
 *    Topic ID registered in the broker, needed to register first in the broker to publish
 *  @var publish_packet_t::message_id
 *    Message identifier - ID
 *  @var publish_packet_t::data
 *    Data to be published in the defined topic
 */
typedef struct __attribute__((packed)){
  uint8_t length;
  uint8_t type;
  uint8_t flags;
  uint16_t topic_id;
  uint16_t message_id;
  char data[MQTT_SN_MAX_PACKET_LENGTH-7];
} publish_packet_t;

/** @struct subscribe_wildcard_packet_t
 *  @brief Packet structure for the subscribe Wildcard mode for the MQTT-SN
 *  @var subscribe_wildcard_packet_t::length
 *    Length of the packet
 *  @var subscribe_wildcard_packet_t::type
 *    Type of message
 *  @var subscribe_wildcard_packet_t::flags
 *    Flags used (retain,DUP,QoS...)
 *  @var subscribe_wildcard_packet_t::message_id
 *    Message identifier - ID used to receive the correspondent packet
 *  @var subscribe_wildcard_packet_t::topic_name
 *    Topic of type wildcard to subscribe
 */
typedef struct __attribute__((packed)) {
  uint8_t length;
  uint8_t type;
  uint8_t flags;
  uint16_t message_id;
  char topic_name[MQTT_SN_MAX_TOPIC_LENGTH];
} subscribe_wildcard_packet_t;

/** @struct subscribe_packet_t
 *  @brief Subscribe packet structure for the MQTT-SN
 *  @var subscribe_packet_t::length
 *    Length of the packet
 *  @var subscribe_packet_t::type
 *    Type of message
 *  @var subscribe_packet_t::flags
 *    Flags used (retain,DUP,QoS...)
 *  @var subscribe_packet_t::message_id
 *    Message identifier - ID used to receive the correspodent packet
 *  @var subscribe_packet_t::topic_id
 *    Topic ID pre-registered with the topic correspondent to the subscription (topic must be inserted in the register list/vector in the beginning)
 */
typedef struct __attribute__((packed)) {
  uint8_t length;
  uint8_t type;
  uint8_t flags;
  uint16_t message_id;
  uint16_t topic_id;
} subscribe_packet_t;

/** @struct connect_packet_t
 *  @brief Packet structure MQTT-SN of type CONNECT
 *  @var connect_packet_t::length
 *    Total length of the packetMQTT-SN
 *  @var connect_packet_t::type
 *    Describe the type of message that'll be send to the broker
 *  @var connect_packet_t::flags
 *    Has all the flag parameters that'll be send like (DUP,QoS,Retain,Will,
 *    CleanSession, TopicType)
 *  @var connect_packet_t::protocol_id
 *    Present just when CONNECT, indicating the protocol version and the name
 *  @var connect_packet_t::duration
 *    Indicates the time duration in seconds that could be until eighteen hours
 */
typedef struct __attribute__((packed)) {
  uint8_t length;
  uint8_t type;
  uint8_t flags;
  uint8_t protocol_id;
  uint16_t duration;
  char client_id[23];
} connect_packet_t;

/** @struct register_packet_t
 *  @brief Packet structure MQTT-SN of type REGISTER
 *  @var register_packet_t::length
 *    Total length of the packet MQTT-SN
 *  @var register_packet_t::type
 *    Describe the type of message that'll be send to the broker
 *  @var register_packet_t::topic_id
 *    Short Topic that'll be used in the packet REGISTER - When send by the node, it'll use 0x0000
 *  @var register_packet_t::message_id
 *    Identifier unique for the REGACK packet that'll be send by the broker usually
 *  @var register_packet_t::topic_name
 *    Name of the topic that'll be registered
 */
typedef struct __attribute__((packed)){
  uint8_t length;
  uint8_t type;
  uint16_t topic_id;
  uint16_t message_id;
  char topic_name[MQTT_SN_MAX_TOPIC_LENGTH];
} register_packet_t;

/** @struct willtopic_packet_t
 *  @brief Packet structure MQTT-SN of type WILL TOPIC
 *  @var willtopic_packet_t::length
 *    Total length of the packet MQTT-SN
 *  @var willtopic_packet_t::type
 *    Describe the type of message that'll be send to the broker
 *  @var willtopic_packet_t::flags
 *    Has all the parameters that'll be send like(DUP,QoS,Retain,Will,
 *    CleanSession, TopicType)
 *  @var willtopic_packet_t::will_topic
 *    Topic where it'll be published the message when the device disconnect (LW teastment)
 */
typedef struct __attribute__((packed)){
  uint8_t length;
  uint8_t type;
  uint8_t flags;
  char will_topic[MQTT_SN_MAX_TOPIC_LENGTH];
} willtopic_packet_t;

/** @struct willmessage_packet_t
 *  @brief Packet structure MQTT-SN of type WILL MESSAGE
 *  @var willmessage_packet_t::length
 *    Total length of the packetMQTT-SN
 *  @var willmessage_packet_t::type
 *    Describe the type of message that'll be send to the broker
 *  @var willmessage_packet_t::will_message
 *    Mesage it'll be published when the device disconnect (LW teastment)
 */
typedef struct __attribute__((packed)){
  uint8_t length;
  uint8_t type;
  char will_message[MQTT_SN_MAX_PACKET_LENGTH];
} willmessage_packet_t;

/** @struct regack_packet_t
 *  @brief Packet structure MQTT-SN of type REGACK
 *  @var regack_packet_t::length
 *    Total length of the packet MQTT-SN
 *  @var regack_packet_t::type
 *    Describe the type of message that'll be send to the broker
 *  @var regack_packet_t::topic_id
 *    Short Topic that'll be used for the packets of type REGISTER - When send by the brker to the node
 *  @var regack_packet_t::message_id
 *    Identifier unique for the REGACK packet that'll be send by the broker usually
 *  @var regack_packet_t::return_code
 *    Return code for the message
 */
typedef struct __attribute__((packed)){
  uint8_t length;
  uint8_t type;
  uint16_t topic_id;
  uint16_t message_id;
  uint8_t return_code;
} regack_packet_t;
/** @}*/

/** @typedef mqtt_sn_cb_f
 *  @brief Callback type of function that'll be called when the broker send something
 */
typedef void (*mqtt_sn_cb_f)(char *,char *);

/** @struct mqtt_sn_task_t
 *  @brief Task structure of queue MQTT-SN
 *  @var mqtt_sn_task_t::msg_type_q
 *    Type of message to be allocated
 *  @var mqtt_sn_task_t::short_topic
 *    Short topic to be published/subscribed
 *  @var mqtt_sn_task_t::qos_level
 *    QoS level of the task to be allocated in the FIFO
 *  @var mqtt_sn_task_t::id_task
 *    Task identifier
 */
typedef struct {
  uint8_t  msg_type_q;
  uint8_t  short_topic;
  uint16_t id_task;
  uint8_t  qos_level;
  uint8_t  retain;
} mqtt_sn_task_t;

/** @struct node
 *  @brief FIFO Queue struct MQTT-SN
 *  @var data
 *    Task to be processed
 *  @var link
 *    Link for the next task
 */
struct node {
    mqtt_sn_task_t data;
    struct node *link;
}*mqtt_queue_first, *mqtt_queue_last;

/** @typedef resp_con_t
 *  @brief Type of function error
 *  @var SUCCESS_CON::FAIL_CON
 *    Error to process something
 *  @var SUCCESS_CON::SUCCESS_CON
 *    Success to process something
 *  @todo implement more types of errors
 */
typedef enum resp_con{
   FAIL_CON,
   SUCCESS_CON,
} resp_con_t;

/** @typedef short_topics_t
 *  @brief Control structure of short type of topics
 */
typedef struct {
   char *topic_name;
   uint8_t short_topic_id;
   uint8_t subscribed;
} short_topics_t;

/** @typedef mqtt_sn_status_t
 *  @brief FSM states of the MQTT-SN
 */
typedef enum {
  MQTTSN_CONNECTION_FAILED,
  MQTTSN_DISCONNECTED,
  MQTTSN_WAITING_CONNACK,
  MQTTSN_WAITING_WILLTOPICREQ,
  MQTTSN_WAITING_WILLMSGREQ,
  MQTTSN_WAITING_REGACK,
  MQTTSN_CONNECTED,
  MQTTSN_TOPIC_REGISTERED,
  MQTTSN_TOPIC_SUBSCRIBING,
  MQTTSN_WAITING_PUBACK,
  MQTTSN_WAITING_SUBACK,
  MQTTSN_PUB_REQ,
  MQTTSN_SUB_REQ,
  MQTTSN_REG_REQ
} mqtt_sn_status_t;

/** @struct mqtt_sn_con_t
 *  @brief Connection structure of the MQTT-SN broker
 *  @var mqtt_sn_con_t::simple_udp_connection
 *    Handle of UDP connection with the broker
 *  @var mqtt_sn_con_t::udp_port
 *    UDP port of connection with the broker (default:1884)
 *  @var mqtt_sn_con_t::ipv6_broker
 *    IPv6 address of the broker UDP
 *  @var mqtt_sn_con_t::keep_alive
 *    Requisition time of Keep Alive for PINGREQ and PINGRESP
 *  @var mqtt_sn_con_t::client_id
 *    Client ID with the broker
 */
typedef struct {
  struct simple_udp_connection udp_con;
  uint16_t udp_port;
  uint16_t *ipv6_broker;
  uint8_t  keep_alive;
  const char* client_id;
  char *will_topic;
  char *will_message;
} mqtt_sn_con_t;

/** @brief Insert a task in the queue
 *
 * 		Insert a new task in the queue to be processed
 *
 *  @param [in] new New task to be processed by the FSM MQTT-SN
 *
 *  @retval FAIL_CON         Cannot be possible allocate a new task in the queue
 *  @retval SUCCESS_CON      Success to allocate a new task in the queue
 *
 * 	@todo		Increase the performance of execution time
 **/
resp_con_t mqtt_sn_insert_queue(mqtt_sn_task_t new);

/** @brief Removes the first of the Queue to process
 *
 * 		Remove the first of the queue to process
 *
 *  @param [in] 0 Doesn't receive args
 *
 *  @retval 0 Doesn't return data
 *
 * 	@todo	add middle exclusion
 **/
void mqtt_sn_delete_queue();

/** @brief List of taks in the queue
 *
 * 		Run for the queue to print the task allocated before
 *
 *  @param [in] 0 Doesn't receive args
 *
 *  @retval 0 Doesn't return data
 *
 **/
void mqtt_sn_check_queue();

/** @brief Send connection requisition to the broker
 *
 * 		Send CONNECT type of messages to the broker MQTT-SN
 *
 *  @param [in] rc Return code of requisition MQTT (Return Code)
 *
 *  @retval FAIL_CON     Fail for some reason in the return code
 *  @retval SUCCESS_CON   Success the return code
 *
 * 	@todo		Expand the return fail type to be more understoodable
 **/
resp_con_t mqtt_sn_check_rc(uint8_t rc);

/** @brief Execute the parsing of receive messages
 *
 * 	 Parse the messages according to the MQTT-SN protocol, changing the connection status with the broker
 *
 *  @param [in] data Pointer to the UDP received data
 *
 *  @retval 0 Doesn't return data
 **/
void mqtt_sn_recv_parser(const uint8_t *data);

/** @brief Initialize the UDP connection with the broker
 *
 *    Stabilizes a connection with the MQTT-SN broker/server, through the 1884 port, besides init. the queue mechanism of taks
 *
 *  @param [in] mqtt_sn_connection Default structure of communication in MQTT-SN
 *  @param [in] topics Vector of registered topics
 *  @param [in] topic_len Size of the vector of topics to be registered
 *  @param [in] mqtt_sn_cb_f Pointer for the callback function that'll process the received messages
 *
 *  @retval FAIL_CON      Fail to stabilishes the UDP connection
 *  @retval SUCCESS_CON   Success to stabilishes the UDP connection
 *
 **/
resp_con_t mqtt_sn_create_sck(mqtt_sn_con_t mqtt_sn_connection, char *topics[],size_t topic_len, mqtt_sn_cb_f cb_f);

/** @brief Envio de mensagens ao broker of type REGISTER
 *
 * 		Send to the broker packets of type REGISTER with the topic name according to the first element in the queue of tasks
 *
 *  @param [in] 0 Doesn't receive parameter
 *
 *  @retval FAIL_CON      Fail to send the REGISTER packet
 *  @retval SUCCESS_CON   Success to send the REGISTER packet
 *
 **/
resp_con_t mqtt_sn_reg_send(void);

/** @brief Check the connection state with the broker MQTT-SN
 *
 * 		Returns the statues of connection based on the structure mqtt_sn_status_t
 *
 *  @param [in] 0 Doesn't receive args
 *
 *  @retval mqtt_sn_status_t State of connection
 *
 **/
mqtt_sn_status_t mqtt_sn_check_status(void);

/** @brief Send requisition to the broker MQTT-SN
 *
 * 		Realizes the send of messages of type CONNECT to the broker MQTT-SN
 *
 *  @param [in] 0 Doesn't receive args
 *
 *  @retval FAIL_CON      Fail to send CONNECT packet
 *  @retval SUCCESS_CON   Success to send CONNECT packet
 *
 **/
resp_con_t mqtt_sn_con_send(void);

/** @brief Check the status of the tasks in the queue
 *
 * 		Run the tasks vector to check the status
 *
 *  @param [in] 0 Doesn't receive args
 *
 *  @retval TRUE  Empty queue
 *  @retval FALSE  There're tasks to be processed
 *
 **/
bool mqtt_sn_check_empty(void);

/** @brief Return a correspondent string to the statue
 *
 * 		Realizes the parse of connections status translating from typedef enum to the state in a string
 *
 *  @param [in] type type of status to translate
 *  @param [in] type_string variable that'll receive the string message
 *
 *  @retval 0 Doesn't return data
 *
 **/
void parse_mqtt_type_string(uint8_t type, char **type_string);

/** @brief Initializes the PROCESS_THREAD MQTT-SN
 *
 * 		Initializes the PROCESS_THREAD of MQTT-SN as other variables used for events
 *
 *  @param [in] 0 Doesn't receive args
 *
 *  @retval 0 Doesn't return data
 *
 **/
void mqtt_sn_init(void);

/** @brief Send PUBLISH packet to the broker MQTT-SN
 *
 * 		Create the packet and send to the broker the publishing
 *
 *  @param [in] topic Topic that'll be published
 *  @param [in] message Message to publish
 *  @param [in] qos QoS level of publication
 *
 *  @retval FAIL_CON      Fail to publish
 *  @retval SUCCESS_CON   Success publish
 *
 **/
resp_con_t mqtt_sn_pub_send(char *topic,char *message, bool retain_flag, uint8_t qos);

/** @brief Check the status of connection in a string mode
 *
 * 		Verify the status of connection and send the status in a string mode
 *
 *  @param [in] Doesn't receive args
 *
 *  @retval STRING  String of the actual status of connection MQTT-SN
 *
 **/
char* mqtt_sn_check_status_string(void);

/** @brief Generate the QoS flag
 *
 * 		Return the flag state correspondent to the QoS sent
 *
 *  @param [in] QoS level desired
 *
 *  @retval QoS Return the desired QoS level flag
 *
 **/
uint8_t mqtt_sn_get_qos_flag(int8_t qos);

/** @brief Prepare the requisiton of publish to the broker MQTT-SN
 *
 * 		Format and generate the task in the queue to publish in the pre-registered topic
 *
 *  @param [in] topic Topic to be published
 *  @param [in] message Message to publish
 *  @param [in] retain_flag Message identifier - ID retain
 *  @param [in] qos QoS level of the publish
 *
 *  @retval FAIL_CON      Fail to generate the task required
 *  @retval SUCCESS_CON   Success to generate the task required
 *
 **/
resp_con_t mqtt_sn_pub(char *topic,char *message, bool retain_flag, uint8_t qos);

/** @brief Shows the registered topics
 *
 * 		Shows the list of registered topics in the broker
 *
 *  @param [in] 0 Doesn't receive args
 *
 *  @retval 0 Doesn't return data
 *
 **/
void print_g_topics(void);

/** @brief Process the timeout of packets
 *
 *    Process all and any req. of expiration by tiemout expiration in the sent messages (SUBSCRIBE, PUBLISH, REGISTER)
 *
 *  @param [in] 0 Doesn't receive args
 *
 *  @retval 0 Doesn't return data
 *
 **/
void timeout_con(void *ptr);

/** @brief Process the timeout of ping
 *
 * 		Process timeout of messages type PINGREQ
 *
 *  @param [in] 0 Doesn't receive args
 *
 *  @retval 0 Doesn't return data
 *
 **/
void timeout_ping_mqtt(void *ptr);

/** @brief Send ping recognition to the broker
 *
 * 		Send the requisition tot he broker directly by PING REQ messages
 *
 *  @param [in] 0 Doesn't receive args
 *
 *  @retval 0 Doesn't return data
 *
 **/
void mqtt_sn_ping_send(void);

/** @brief Release the generate task option
 *
 * 	Enables the generation of tasks according to the MQTT-SN status of connection
 *
 *  @param [in] 0 Doesn't receive args
 *
 *  @retval TRUE Can generate task in the queue
 *  @retval FALSE MQTT-SN status connection cannot generate tasks
 *
 **/
bool unlock_tasks(void);

/** @brief Prepares the requisition to subscribe to the broker MQTT-SN
 *
 * 		Format and generate the tasks in the queue to subscribe of pre-registered topic
 *
 *  @param [in] topic Topic to subscribe
 *  @param [in] qos QoS level of subscription
 *
 *  @retval FAIL_CON      Fail to generate the sub. task
 *  @retval SUCCESS_CON   Success to generate the sub. task
 *
 **/
resp_con_t mqtt_sn_sub(char *topic, uint8_t qos);

/** @brief Send packet of type SUBSCRIBE to broker MQTT-SN
 *
 * 		Create and send the packet of type subscribe to the broker
 *
 *  @param [in] topic Topic to subscribe (must be pre-registered and passed to mqtt_sn_create_sck)
 *  @param [in] qos QoS level of publish
 *
 *  @retval FAIL_CON      Fail to send a subscribe
 *  @retval SUCCESS_CON   Success to send a subscribe
 *
 **/
resp_con_t mqtt_sn_sub_send(char *topic, uint8_t qos);

/** @brief Send the packet of SUBSCRIBE of type WILDCARD to the broker MQTT-SN
 *
 * 		Create the packet and send to the broker a message of subscribe of type Wildcard (#,+)
 *
 *  @param [in] topic Topic to subscribe (must be pre-registered and passed to mqtt_sn_create_sck)
 *  @param [in] qos QoS level of publish
 *
 *  @retval FAIL_CON      Fail to send a subscribe
 *  @retval SUCCESS_CON   Success to send a subscribe
 *
 **/
resp_con_t mqtt_sn_sub_send_wildcard(char *topic, uint8_t qos);

/** @brief Check if the topic has been already registered
 *
 *    Check if the topic in analisis has beedn already registered or it's in the register process
 *
 *  @param [in] topic Topic to subscribe (must be pre-registered and passed to mqtt_sn_create_sck)
 *
 *  @retval FAIL_CON      Topic has been registered or it's in the register process
 *  @retval SUCCESS_CON   Topic release to subscribe
 *
 **/
resp_con_t verf_hist_sub(char *topic);

/** @brief Send the packet DISCONNECT to broker MQTT-SN
 *
 * 		Create the packet and send the broker a disconnection message
 *
 *  @param [in] duration Time that'll be disconnected, used for sleeping devices
 *
 *  @retval FAIL_CON      Fail to send disconnection
 *  @retval SUCCESS_CON   Success to send disconnection
 *
 **/
resp_con_t mqtt_sn_disconnect_send(uint16_t duration);

/** @brief Initializes the MQTT-SN vectors
 *
 * 		Delete previous tasks in the queue and init. the topic vectors setting 0xFF and topic identifiers
 *
 *  @param [in] 0 Doesn't receive args
 *
 *  @retval 0 Doesn't return arguments
 *
 **/
void init_vectors(void);

/** @brief Init. the event SUBSCRIBE
 *
 * 		Init the subscription of events
 *
 *  @param [in] 0 Doesn't receive args
 *
 *  @retval 0 Doesn't return arguments
 *
 **/
void init_sub(void *ptr);

/** @brief Verify the pre-registered of the topic
 *
 *    Verify if the topic has beend registered before init. the any operation
 *
 *  @param [in] topic Topic to be verified
 *
 *  @retval FAIL_CON      Topic was not registered
 *  @retval SUCCESS_CON   Topic was registered
 *
 **/
resp_con_t verf_register(char *topic);

/** @brief Send LWT message
 *
 * 		Send the message LWT when disconnect occurs
 *
 *  @param [in] 0 Doesn't receive args
 *
 *  @retval FAIL_CON      Fail to send a message Will MSG
 *  @retval SUCCESS_CON   Success to send a message Will MSG
 *
 **/
resp_con_t mqtt_sn_will_message_send(void);

/** @brief Send the topic LWT
 *
 * 		Send the topic used when the device disconnect itself
 *
 *  @param [in] 0 Doesn't receive args
 *
 *  @retval FAIL_CON      Fail to send a message Will MSG
 *  @retval SUCCESS_CON   Success to send a message Will MSG
 *
 **/
resp_con_t mqtt_sn_will_topic_send(void);

/** @brief Callback of UDP reception
 *
 * 		Receive data of connection UDP with the broker
 *
 *  @param [in] simple_udp_connection Handle of UDP connection
 *  @param [in] sender_addr IP address of the broker
 *  @param [in] sender_port Port of send
 *  @param [in] receiver_addr Reception address
 *  @param [in] receiver_port Reception port
 *  @param [in] data Data received
 *  @param [in] datalen Size of the data received
 *
 *  @retval 0 Doens't return data
 *
 **/
void mqtt_sn_udp_rec_cb(struct simple_udp_connection *c,
                            const uip_ipaddr_t *sender_addr,
                            uint16_t sender_port,
                            const uip_ipaddr_t *receiver_addr,
                            uint16_t receiver_port,
                            const uint8_t *data,
                            uint16_t datalen);

/** @brief Prepares the SUBSCRIBE task of type WILDCARD
 *
 * 		Generate the tasks of subscription with Wildcard to the broker MQTT-SN
 *
 *  @param [in] topic Topic to subscribe (must be pre-registered and passed to mqtt_sn_create_sck)
 *  @param [in] qos QoS level of publish
 *
 *  @retval FAIL_CON      Fail to send a subscribe
 *  @retval SUCCESS_CON   Success to send a subscribe
 *
 **/
resp_con_t mqtt_sn_sub_wildcard(char *topic, uint8_t qos);

/** @brief Send the packet of type REGACK to the broker
 *
 * 		Send to the broker MQTT-SN a message of type REGACK when it receives any topic through Wildcad
 *
 *  @param [in] msg_id Message id correspondent to the send
 *  @param [in] topic_id Topic ID send by the broker to register a new vector of topics
 *
 *  @retval FAIL_CON      Fail to send a regack
 *  @retval SUCCESS_CON   Success to send a regack
 *
 **/
resp_con_t mqtt_sn_regack_send(uint16_t msg_id, uint16_t topic_id);

#endif
