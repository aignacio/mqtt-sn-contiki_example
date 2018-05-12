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
 * @file mqtt_sn.c
 * @author Ânderson Ignácio da Silva
 * @date 19 Ago 2016
 * @brief Main file with the functions of the MQTT-SN port for Contiki
 * @see http://www.aignacio.com
 * @todo Change the callback function of received packets from a pointer to a data and the length besides use a for


 [Annotation n-1]:
 If you're using contiki-os pay attention that always a event timer expires,
 this event variable will be expired forever if you doesn't cleanup it, so beware
 when you have construction in your code like those below:
 1) else if(ev == etimer_expired(....))
 2) else if(ev == mqtt_event_regack)
 ...
 In this condition, you'll never enter in the 2) if the first expire once first.

 [Annotation n-2]:
 As this API function uses malloc we need the POSIX function called "sbrk" so to add
 this wee need to add the file syscall.c, available in the contiki's folder (lib/newlib/syscalls.c)
 e also we need to include in the linker script (cc26xx.ld) the functions _heap and
 _eheap as the follows in the end of the linker.

   _heap = .;
   _eheap = ORIGIN(SRAM) + LENGTH(SRAM);
  }
*/

#include "contiki.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "simple-udp.h"
#include <stdio.h>
#include <string.h>
#include <mqtt_sn.h>
#include "sys/timer.h"
#include "list.h"
#include "sys/ctimer.h"
#include "sys/etimer.h"
#include "stdint.h"
#include <stdlib.h>
#include <stdbool.h>
#include "net/ipv6/uip-ds6.h"
#if CONTIKI_TARGET_SRF06_CC26XX
#include "lib/newlib/syscalls.c" //Used because malloc
#endif

static struct ctimer              mqtt_time_connect;       // Time structure to send CONNECT
static struct ctimer              mqtt_time_register;      // Time structure to send REGISTER
static struct ctimer              mqtt_time_ping;          // Time structure to send PING
static struct ctimer              mqtt_time_subscribe;     // Time structure to send SUBSCRIBE

static process_event_t            mqtt_event_connect;          // Event of req. CONNECT  [node --> broker]
static process_event_t            mqtt_event_connack;          // Event of req. CONNACK  [broker --> node]
static process_event_t            mqtt_event_register;         // Event of req. REGISTER [node --> broker]
static process_event_t            mqtt_event_regack;           // Event of req. REGACK   [broker --> node]
static process_event_t            mqtt_event_pub_qos_0;        // Event of req. PUBLISH - QoS - 0 [node --> broker]
static process_event_t            mqtt_event_subscribe;        // Event of req. SUBSCRIBE  [node --> broker]
static process_event_t            mqtt_event_suback;           // Event of req. SUBACK  [broker --> node]
static process_event_t            mqtt_event_run_task;         // Event of req. any as PUBLISH or SUBSCRIBE [node --> broker]
static process_event_t            mqtt_event_ping_timeout;     // Event of req. PING REQUEST in timeout by the broker [broker <-> node]
static process_event_t            mqtt_event_connected;        // Event of req. PING REQUEST when it's connected [broker <-> node]
static process_event_t            mqtt_event_will_topicreq;    // Event of req. WILL TOPIC REQUEST [broker <-> node]
static process_event_t            mqtt_event_will_messagereq;  // Event of req. WILL MESSAGE REQUEST [broker <-> node]

// Commented because broker doesn't send PUBACK in QoS 0
// static process_event_t            mqtt_event_puback;     // Event of req. PUBACK   [broker --> node]
static bool                       g_recon = false;                   // Identifier of reconnection avoiding double open port UDP
static bool                       g_will = false;                    // Identifier of LWT usage
static bool                       g_ping_flag_resp = true;           // Identifier of PING REQUEST answer
static char                       *g_message_bind;                   // Temporary buffer for publish tasks
static char                       *topic_temp_wildcard;              // Temporary buffer for subscriptions of wildcard type
static uint8_t                    g_tries_send = 0;                  // Identifier of send tries
static uint8_t                    g_tries_ping = 0;                  // Identifier of send PING REQUEST
static uint8_t                    g_task_id = 0;                     // Identifier unit for incremental tasks
static short_topics_t             g_topic_bind[MAX_TOPIC_USED];      // Vector to store the bind relationship with short and topic id
static mqtt_sn_con_t              g_mqtt_sn_con;                     // Struct of connection MQTT-SN
static mqtt_sn_status_t           mqtt_status = MQTTSN_DISCONNECTED; // Main FINITE STATE MACHINE of MQTT-SN
static char                       *topics_reconnect[MAX_TOPIC_USED]; // Topic's vector[reconnection]
static uint16_t                   topics_len;                        // Length of total topic's [reconnection]
static mqtt_sn_cb_f               callback_mqtt;

PROCESS(mqtt_sn_main, "[MQTT-SN] Initial process");

/*********************** FUNÇÕES AUXILIARES MQTT-SN ***************************/
bool unlock_tasks(void) {
  if (mqtt_status == MQTTSN_TOPIC_REGISTERED)
    return true;
  return false;
}

void init_sub(void *ptr){
  debug_mqtt("Initializing SUBSCRIBE");
  process_post(&mqtt_sn_main,mqtt_event_run_task,NULL);
}

void parse_mqtt_type_string(uint8_t type, char **type_string){
  switch (type) {
    case MQTT_SN_TYPE_CONNECT:
      *type_string = "CONNECT";
    break;
    case MQTT_SN_TYPE_REGISTER:
      *type_string = "REGISTER";
    break;
    case MQTT_SN_TYPE_SUB_WILDCARD:
      *type_string = "SUBSCRIBE_WILDCARD";
    break;
    case MQTT_SN_TYPE_PUBLISH:
      *type_string = "PUBLISH";
    break;
    case MQTT_SN_TYPE_SUBSCRIBE:
      *type_string = "SUBSCRIBE";
    break;
    case MQTT_SN_TYPE_PINGREQ:
      *type_string = "PINGREQ";
    break;
    case MQTT_SN_TYPE_PINGRESP:
      *type_string = "PINGRESP";
    break;
    case MQTT_SN_TYPE_DISCONNECT:
      *type_string = "DISCONNECT";
    break;
    case MQTT_SN_TYPE_WILLTOPIC:
      *type_string = "WILL_TOPIC";
    break;
    case MQTT_SN_TYPE_WILLMSG:
      *type_string = "WILL_MESSAGE";
    break;
    default:
      *type_string = "None of available options";
    break;
  }
}

char* mqtt_sn_check_status_string(void){
  switch (mqtt_status) {
    case MQTTSN_DISCONNECTED:
      return "DISCONNECTED";
    break;
    case MQTTSN_WAITING_CONNACK:
      return "WAITING FOR CONNACK";
    break;
    case MQTTSN_WAITING_REGACK:
      return "WAITING FOR REGACK";
    break;
    case MQTTSN_CONNECTED:
      return "#### CONNECTED ####";
    break;
    case MQTTSN_TOPIC_REGISTERED:
      return "TOPICS REGISTERED";
    break;
    case MQTTSN_WAITING_WILLTOPICREQ:
      return "WAITING FOR WILL TOPIC";
    break;
    case MQTTSN_WAITING_WILLMSGREQ:
      return "WAITING FOR WILL MESSAGE";
    break;
    default:
      return "None of available options";
    break;
  }
}

mqtt_sn_status_t mqtt_sn_check_status(void){
  return mqtt_status;
}

resp_con_t mqtt_sn_check_rc(uint8_t rc){
  switch (rc) {
    case ACCEPTED:
      return SUCCESS_CON;
    break;
    case REJECTED_CONGESTION:
      return FAIL_CON;
    break;
    case REJECTED_INVALID_TOPIC_ID:
      return FAIL_CON;
    break;
    case REJECTED_NOT_SUPPORTED:
      return FAIL_CON;
    break;
    default:
      return FAIL_CON;
    break;
  }
}

uint8_t mqtt_sn_get_qos_flag(int8_t qos){
    switch (qos) {
        case -1:
          return MQTT_SN_FLAG_QOS_N1;
        case 0:
          return MQTT_SN_FLAG_QOS_0;
        case 1:
          return MQTT_SN_FLAG_QOS_1;
        case 2:
          return MQTT_SN_FLAG_QOS_2;
        default:
          return 0;
    }
}

resp_con_t mqtt_sn_sub_wildcard(char *topic, uint8_t qos){
  mqtt_sn_task_t subscribe_task;

  subscribe_task.msg_type_q      = MQTT_SN_TYPE_SUB_WILDCARD;
  subscribe_task.qos_level       = qos;
  topic_temp_wildcard            = topic;
  if (!mqtt_sn_insert_queue(subscribe_task))
   debug_task("Erro to add on the task queue!");

  return SUCCESS_CON;
}

resp_con_t mqtt_sn_sub(char *topic, uint8_t qos){
  // If there's topics to register, doesn't enables the subscription
  // avoiding some crossover between the process. Where tasks have more
  // priority than subscription by the user.
  // if (!unlock_tasks())
  // return FAIL_CON;

  if(strstr(topic,"#") || strstr(topic,"+")){
    mqtt_sn_sub_wildcard(topic,qos);
    return SUCCESS_CON;
  }

  if (!verf_register(topic))
  return FAIL_CON;

  if(verf_hist_sub(topic)){
    mqtt_sn_task_t subscribe_task;
    size_t i;
    for (i=0; i < MAX_TOPIC_USED; i++)
      if(strcmp(g_topic_bind[i].topic_name,topic) == 0) // It's a new or old topic?
        break;

    subscribe_task.msg_type_q      = MQTT_SN_TYPE_SUBSCRIBE;
    subscribe_task.qos_level       = qos;
    subscribe_task.short_topic     = i;

    if (!mqtt_sn_insert_queue(subscribe_task))
     debug_task("ERRO AO ADICIONAR NA FILA");

    return SUCCESS_CON;
  }
  else
    return FAIL_CON;

}

resp_con_t mqtt_sn_pub(char *topic,char *message, bool retain_flag, uint8_t qos){
  // If there're topics to register, doesn't enables the publish avoiding conflicts
  // during this process, so tasks has more prority than directly subscriptions
  if (!unlock_tasks())
    return FAIL_CON;

  // We analize the buffer of topícs REGISTERED to check if it has been already registered before
  if (!verf_register(topic))
    return FAIL_CON;

  mqtt_sn_pub_send(topic,message,retain_flag,qos);
  return SUCCESS_CON;
}

resp_con_t verf_hist_sub(char *topic){
  size_t i;

  for (i=0; i < MAX_TOPIC_USED; i++)
    if(strcmp(g_topic_bind[i].topic_name,topic) == 0)
      break;

  if (g_topic_bind[i].subscribed == 0x01){  // Na fila para inscrever? 0x01?
    debug_mqtt("Subscribing to:[%s]",g_topic_bind[i].topic_name);
    return FAIL_CON;
  }
  else if (g_topic_bind[i].subscribed == 0x02) {  // Já inscrito? 0x02?
    debug_mqtt("Topic subscribed:[%s]",g_topic_bind[i].topic_name);
    return FAIL_CON;
  }
  else if (g_topic_bind[i].subscribed == 0x00){  // Caso g_topic_bind[i].subscribed == 0x00 vamos preparar o topico para ser inscrito
    g_topic_bind[i].subscribed = 0x01;
    debug_mqtt("Waiting for subscription:[%s]",g_topic_bind[i].topic_name);
    return SUCCESS_CON;
  }
  else{
    debug_mqtt("Strange value on SUBSCRIBED:%d",g_topic_bind[i].subscribed);
    return FAIL_CON;
  }

}

resp_con_t verf_register(char *topic){
  size_t i;
  for (i=0; i < MAX_TOPIC_USED; i++)
    if(strcmp(g_topic_bind[i].topic_name,topic) == 0)  // Tópico novo ou existe?
      return SUCCESS_CON;

  debug_mqtt("Topic not registered!");
  return FAIL_CON;
}

void print_g_topics(void){
  size_t i;
  debug_mqtt("Vector of topics");
  for(i = 0 ; g_topic_bind[i].short_topic_id != 0xFF; i++) {
    debug_mqtt("[i=%d][%d][%s]",i,g_topic_bind[i].short_topic_id,g_topic_bind[i].topic_name);
  }
}

void init_vectors(void){
  debug_mqtt("Initializing vectors...");
  size_t i;
  for (i = 1; i < MAX_TOPIC_USED; i++){
    g_topic_bind[i].short_topic_id = 0xFF;
    g_topic_bind[i].topic_name = 0;
    g_topic_bind[i].subscribed = 0x00;
  }

  while (!mqtt_sn_check_empty())
      mqtt_sn_delete_queue();
  g_task_id = 0;
}

/******************** FUNÇÕES DE ENVIO DE PacketS MQTT-SN *********************/
resp_con_t mqtt_sn_will_topic_send(void){
  willtopic_packet_t packet;

  size_t topic_name_len = strlen(g_mqtt_sn_con.will_topic);

  if (topic_name_len > MQTT_SN_MAX_TOPIC_LENGTH) {
    debug_mqtt("Error: Max. length in WILL topic name");
    return FAIL_CON;
  }

  packet.flags = MQTT_SN_FLAG_RETAIN;

  packet.type = MQTT_SN_TYPE_WILLTOPIC;

  strncpy(packet.will_topic, g_mqtt_sn_con.will_topic, topic_name_len);
  packet.length = 0x03 + topic_name_len;
  packet.will_topic[topic_name_len] = '\0';

  debug_mqtt("Sending the packet @WILL TOPIC");
  simple_udp_send(&g_mqtt_sn_con.udp_con,&packet, packet.length);

  return SUCCESS_CON;
}

resp_con_t mqtt_sn_will_message_send(void){
  willmessage_packet_t packet;

  size_t message_name_len = strlen(g_mqtt_sn_con.will_message);

  if (message_name_len > MQTT_SN_MAX_TOPIC_LENGTH) {
    debug_mqtt("Error: Max. length in WILL message");
    return FAIL_CON;
  }

  packet.type = MQTT_SN_TYPE_WILLMSG;

  strncpy(packet.will_message, g_mqtt_sn_con.will_message, message_name_len);
  packet.length = 0x02 + message_name_len;
  packet.will_message[message_name_len] = '\0';

  debug_mqtt("Sending the packet @WILL MESSAGE");
  simple_udp_send(&g_mqtt_sn_con.udp_con,&packet, packet.length);

  return SUCCESS_CON;
}

void mqtt_sn_ping_send(void){
  ping_req_t ping_request;

  ping_request.msg_type = MQTT_SN_TYPE_PINGREQ;
  strncpy(ping_request.client_id, g_mqtt_sn_con.client_id, strlen(g_mqtt_sn_con.client_id));
  ping_request.client_id[strlen(g_mqtt_sn_con.client_id)] = '\0';
  //debug_mqtt("Client ID PING:%s",ping_request.client_id);
  ping_request.length = 0x02 + strlen(ping_request.client_id);
  //debug_mqtt("Sending @PINGREQ");
  simple_udp_send(&g_mqtt_sn_con.udp_con,&ping_request, ping_request.length);
}

resp_con_t mqtt_sn_con_send(void){
  connect_packet_t packet;

  // Creating the CONNECT packet
  packet.type = MQTT_SN_TYPE_CONNECT;
  packet.flags = MQTT_SN_FLAG_CLEAN;
  if (g_will)
    packet.flags += MQTT_SN_FLAG_WILL;
  packet.protocol_id = MQTT_SN_PROTOCOL_ID;
  packet.duration = uip_htons(g_mqtt_sn_con.keep_alive); //Realize the conversion to network byte order

  strncpy(packet.client_id, g_mqtt_sn_con.client_id, strlen(g_mqtt_sn_con.client_id));
  packet.client_id[strlen(g_mqtt_sn_con.client_id)] = '\0';
  packet.length = 0x06 + strlen(packet.client_id);

  // debug_mqtt("CLIENT_ID:%s, Tamanho:%d",packet.client_id,strlen(packet.client_id));
  debug_mqtt("Sending the packet @CONNECT ");
  simple_udp_send(&g_mqtt_sn_con.udp_con,&packet, packet.length);
  // debug_mqtt("enviado!");
  return SUCCESS_CON;
}

resp_con_t mqtt_sn_reg_send(void){
  register_packet_t packet;

  size_t i = 0;
  for (i=1; i < MAX_TOPIC_USED; i++)
    if (g_topic_bind[i].short_topic_id == 0xFF)
      break;

  uint16_t task_id = i;
  // Leak of memory
  //size_t topic_name_len = strlen(mqtt_queue_first->data.long_topic); //Get the first of the tasks WAITING FOR
  size_t topic_name_len = strlen(g_topic_bind[task_id].topic_name);

  if (topic_name_len > MQTT_SN_MAX_TOPIC_LENGTH) {
    debug_mqtt("Error: Name of the topic exceeds the max length");
    return FAIL_CON;
  }

  if (mqtt_queue_first->data.msg_type_q != MQTT_SN_TYPE_REGISTER) {
    debug_mqtt("Error: Packet to process is not of the type REGISTER");
    return FAIL_CON;
  }

  packet.type = MQTT_SN_TYPE_REGISTER;
  packet.topic_id = 0x0000;

  // When the broker answer with the short topic ID, it'll use a message id, the identifier of the task
  // in the queue of services MQTT-SN, so it'll be easier to mount the the relationship between short and long topic id
  // (short_topic/long_topic) no vetor global g_topic_bind[]
  packet.message_id = uip_htons(task_id);

  strncpy(packet.topic_name, g_topic_bind[task_id].topic_name, topic_name_len);
  packet.length = 0x06 + topic_name_len;
  packet.topic_name[topic_name_len] = '\0';

  debug_mqtt("Topic to register:%s [%d][MSG_ID:%d]",packet.topic_name,strlen(packet.topic_name),(int)mqtt_queue_first->data.id_task);
  debug_mqtt("Sending the packet @REGISTER");
  simple_udp_send(&g_mqtt_sn_con.udp_con,&packet, packet.length);

  return SUCCESS_CON;
}

resp_con_t mqtt_sn_regack_send(uint16_t msg_id, uint16_t topic_id){
  regack_packet_t packet;

  packet.type = MQTT_SN_TYPE_REGACK;
  packet.topic_id = uip_htons(topic_id);
  packet.message_id = uip_htons(msg_id);
  packet.return_code = 0x00;
  packet.length = 0x07;

  debug_mqtt("Sending the packet @REGACK");
  simple_udp_send(&g_mqtt_sn_con.udp_con,&packet, packet.length);
  return SUCCESS_CON;
}

resp_con_t mqtt_sn_pub_send(char *topic,char *message, bool retain_flag, uint8_t qos){
  publish_packet_t packet;
  uint16_t stopic = 0x0000;
  uint8_t data_len = strlen(message);

  size_t i = 0;
  for (i=0; i < MAX_TOPIC_USED; i++)
    if (strcmp(g_topic_bind[i].topic_name,topic) == 0) {
      stopic = g_topic_bind[i].short_topic_id;
      break;
    }

  if (data_len > sizeof(packet.data)) {
      printf("Error: Payload is too big!!\n");
      return FAIL_CON;
  }

  packet.type  = MQTT_SN_TYPE_PUBLISH;
  packet.flags = 0x00;

  if (retain_flag)
    packet.flags += MQTT_SN_FLAG_RETAIN;

  packet.flags += mqtt_sn_get_qos_flag(qos);

  // According to the official specification:
  // TopicIdType: indicates whether the field TopicId or TopicName included in this message contains a normal
  // topic id (set to “0b00”), a pre-defined topic id (set to “0b01”), or a short topic name (set to “0b10”). The
  // value “0b11” is reserved. Refer to sections 3 and 6.7 for the definition of the various types of topic ids.
  packet.flags += MQTT_SN_TOPIC_TYPE_NORMAL; //Utilizes the already used topic id

  packet.topic_id = uip_htons(stopic);
  packet.message_id = uip_htons(0x00); // Import just if QoS > 0
  strncpy(packet.data, message, data_len+1);
  //
  // PUBLISH Packet
  //  _________________ ______________________ ___________ ________________ _________________________
  // | Length - 0 | Type of message - 1 | Flags - 2 | Topic ID - 3,4 | Msg ID - 5,6 | Data - 7,n ....|
  // |____________|_____________________|___________ ________________|______________|________________|
  //
  packet.length = 0x07 + (data_len+1);

  debug_mqtt("Sending the packet @PUBLISH");
  // debug_mqtt("Sending the packet @PUBLISH - Task:[%d]",(int)mqtt_queue_first->data.id_task);
  simple_udp_send(&g_mqtt_sn_con.udp_con,&packet, packet.length);
  return SUCCESS_CON;
}

resp_con_t mqtt_sn_sub_send(char *topic, uint8_t qos){
  subscribe_packet_t packet;
  uint16_t stopic = 0x0000;

  size_t i = 0;
  for (i=0; i < MAX_TOPIC_USED; i++)
    if (strcmp(g_topic_bind[i].topic_name,topic) == 0) {
      stopic = g_topic_bind[i].short_topic_id;
      break;
    }

  packet.type  = MQTT_SN_TYPE_SUBSCRIBE;
  packet.flags = 0x00;

  packet.flags += mqtt_sn_get_qos_flag(0);
  packet.message_id = uip_htons(stopic);
  packet.topic_id =  uip_htons(stopic);
  packet.flags += MQTT_SN_TOPIC_TYPE_PREDEFINED; // Utilizes the already registered topic
  packet.length = 0x07;
  //
  //  Packet SUBSCRIBE
  //  _________________ ______________________ ___________ ________________ _______________________________
  // | Length - 0 | Type of message - 1 | Flags - 2 | Msg ID - 3,4  | Topic ID - 5,6 or Topic name 5,n ....|
  // |____________|_____________________|___________|_______________|______________________________________|
  //
  debug_mqtt("Sending the packet @SUBSCRIBE");
  simple_udp_send(&g_mqtt_sn_con.udp_con,&packet, packet.length);
  return SUCCESS_CON;
}

resp_con_t mqtt_sn_sub_send_wildcard(char *topic, uint8_t qos){
  subscribe_wildcard_packet_t packet;

  // We analize the buffer of REGISTERED topics to check if has been already registered
  size_t i = 0;
  for (i=0; i < MAX_TOPIC_USED; i++){
    if (g_topic_bind[i].short_topic_id == 0xFF)
      break;
  }

  packet.type  = MQTT_SN_TYPE_SUBSCRIBE;
  packet.flags = 0x00;

  packet.flags += mqtt_sn_get_qos_flag(0);
  packet.message_id = uip_htons(i);
  strncpy(packet.topic_name,topic,strlen(topic));
  packet.flags += MQTT_SN_TOPIC_TYPE_NORMAL;
  packet.length = 0x05+strlen(topic);

  //
  //  Packet SUBSCRIBE
  //  _________________ ______________________ _____ ________________ _____________________________________
  // | Length - 0 | Type of message - 1 | Flags - 2 | Msg ID - 3,4  | Topic ID - 5,6 ou Topic name 5,n ....|
  // |____________|_____________________|___________|_______________|______________________________________|
  //
  debug_mqtt("Sending the packet @SUBSCRIBE(Wildcard)");
  simple_udp_send(&g_mqtt_sn_con.udp_con,&packet, packet.length);
  return SUCCESS_CON;
}

resp_con_t mqtt_sn_disconnect(uint16_t duration){
  disconnect_packet_t packet;

  packet.msg_type = MQTT_SN_TYPE_DISCONNECT;
  packet.duration = uip_htons(duration);
  packet.length = 0x04;
  debug_mqtt("Disconnecting from the broker...");

  simple_udp_send(&g_mqtt_sn_con.udp_con,&packet, packet.length);
  return SUCCESS_CON;
}

/************************** QUEUE FUNCTIONS MQTT-SN ***************************/
resp_con_t mqtt_sn_insert_queue(mqtt_sn_task_t new){
  struct node *temp,*temp2;

  temp2 = mqtt_queue_first;
  int cnt = 0;
  while (temp2) {
      temp2 = temp2->link;
      cnt++;
  }

  //Max length of the queue
  if (cnt > MAX_QUEUE_MQTT_SN)
    return FAIL_CON;

  temp = (struct node *)malloc(sizeof(struct node));
  temp->data.msg_type_q  = new.msg_type_q;
  temp->data.short_topic = new.short_topic;
  temp->data.retain      = new.retain;
  temp->data.qos_level   = new.qos_level;
  temp->data.id_task     = g_task_id;
  g_task_id++;

  temp->link = NULL;
  if (mqtt_queue_last  ==  NULL) {
      mqtt_queue_first = mqtt_queue_last = temp;
  }
  else {
      mqtt_queue_last->link = temp;
      mqtt_queue_last = temp;
  }

  char *task_type;
  //parse_mqtt_type_string(mqtt_queue_first->data.msg_type_q,&task_type_2);
  //debug_task("Task principal:[%2.0d][%s]",(int)mqtt_queue_first->data.id_task,task_type_2);
  parse_mqtt_type_string(temp->data.msg_type_q,&task_type);
  debug_task("Taks added:[%2.0d][%s]",(int)temp->data.id_task, task_type);
  return SUCCESS_CON;
}

void mqtt_sn_delete_queue(void){
  struct node *temp;
  char *task_type;

  temp = mqtt_queue_first;
  if (mqtt_queue_first->link == NULL) {
      g_task_id = 0;
      parse_mqtt_type_string(mqtt_queue_first->data.msg_type_q,&task_type);
      debug_task("Task removed:[%2.0d][%s]",(int)mqtt_queue_first->data.id_task,task_type);
      debug_task("Task info: Empty");
      mqtt_queue_first = mqtt_queue_last = NULL;
  }
  else {
      g_task_id--;
      parse_mqtt_type_string(mqtt_queue_first->data.msg_type_q,&task_type);
      debug_task("Task removed:[%2.0d][%s]",(int)mqtt_queue_first->data.id_task,task_type);
      mqtt_queue_first = mqtt_queue_first->link;
      free(temp);
  }
}

void mqtt_sn_check_queue(void){
  int cnt = 0;
  struct node *temp;
  char *task_type;

  temp = mqtt_queue_first;

  debug_task("GLOBAL ID of tasks g_task_id:%d",g_task_id);

  debug_task("QUEUE:");
  while (temp) {
      parse_mqtt_type_string(temp->data.msg_type_q,&task_type);
      debug_task("[%2.0d][%s][%d]",(int)temp->data.id_task, task_type,temp->data.short_topic);
      temp = temp->link;
      cnt++;
  }
  debug_task("Size of the queue:[%d]", cnt);
}

bool mqtt_sn_check_empty(void){
  if (mqtt_queue_first  ==  NULL)
    return true;
  else
    return false;
}

/************** MANAGE CONNECTION FUNCTIONS MQTT-SN ********************/
void mqtt_sn_recv_parser(const uint8_t *data){
    uint8_t msg_type = data[1],
            return_code = 0xFF,
            short_topic;
    size_t i = 0;
    // As the MsgType  doens't change the of its position, we test it before the returning code as it can vary
    switch (msg_type) {
      case MQTT_SN_TYPE_CONNACK:
        return_code = data[2]; //In the case of CONNACK - RC[2]
        if (mqtt_sn_check_rc(return_code)){
          if (mqtt_status == MQTTSN_WAITING_CONNACK)
            process_post(&mqtt_sn_main, mqtt_event_connack, NULL);
          else
            debug_mqtt("Received CONNAC without requisition!");
        }
      break;
      case MQTT_SN_TYPE_REGACK:
        return_code = data[6]; //In the case of REGACK - RC[6]
        short_topic = data[3];
        // Indeed, the bytes of short topic are the [2] and [3], however
        // we're just using [3] because we cannot consider more than
        // 15 topics
        if (mqtt_sn_check_rc(return_code)){
          for (i = 0;i < MAX_TOPIC_USED; i++) { //Compare the less byte of MSG ID to atribute to short topic the correct REGISTER
            if (i == data[5]){
              g_topic_bind[i].short_topic_id = short_topic;
            }
          }
          if (mqtt_queue_first->data.msg_type_q == MQTT_SN_TYPE_REGISTER &&
              mqtt_status == MQTTSN_WAITING_REGACK)
            process_post(&mqtt_sn_main, mqtt_event_regack, NULL);
          else
            debug_mqtt("Received REGACK without requisition!");
        }
      break;
      case MQTT_SN_TYPE_PUBACK:
      break;
      case MQTT_SN_TYPE_SUBACK:
        return_code = data[7]; //In the case of SUBACK - RC[7]
        short_topic = data[4];
        // Indeed, the bytes of short topic are the [2] and [3], however
        // we're just using [3] because we cannot consider more than
        // 15 topics
        debug_mqtt("Received SUBACK");

        if (mqtt_sn_check_rc(return_code))
          if (short_topic != 0x00) {
            debug_mqtt("Subscription recognitized:[%s]",g_topic_bind[short_topic].topic_name);
            g_topic_bind[short_topic].subscribed = 0x02;
            if (mqtt_queue_first->data.msg_type_q == MQTT_SN_TYPE_SUBSCRIBE &&
                mqtt_status == MQTTSN_WAITING_SUBACK)
              process_post(&mqtt_sn_main, mqtt_event_suback, NULL);
            else
              debug_mqtt("Received SUBACK without requisition!");
          }
          else{
            debug_mqtt("Received SUBACK of WILDCARD");
            if (mqtt_queue_first->data.msg_type_q == MQTT_SN_TYPE_SUB_WILDCARD){
              mqtt_status = MQTTSN_TOPIC_REGISTERED;
              mqtt_sn_delete_queue();
            }
          }
        else
          debug_mqtt("Error: incorrect returning code");
      break;
      case MQTT_SN_TYPE_PINGRESP:
        g_ping_flag_resp = true;
        //debug_mqtt("Ping respondido");
      break;
      case MQTT_SN_TYPE_PINGREQ:
        mqtt_sn_ping_send();
      break;
      case MQTT_SN_TYPE_PUBLISH:
        debug_mqtt("Received publish:");
        uint8_t message_length = data[0]-7;
        //uint8_t msg_id = data[6];
        short_topic = data[4];
        char message[MQTT_SN_MAX_PACKET_LENGTH];
        // debug_mqtt("[Msg_ID][%d]/[Topic ID][%d]",msg_id,short_topic);

        size_t i;
        for (i = 0; i < (message_length); i++)
          message[i] = data[i+7];
        message[i] = '\0';
        // debug_mqtt("Topico:%s",g_topic_bind[short_topic].topic_name);
        // debug_mqtt("Mensagem:%s",message);
        // debug_mqtt("\n");
        (*callback_mqtt)(g_topic_bind[short_topic].topic_name, message);
      break;
      case MQTT_SN_TYPE_REGISTER:
        debug_mqtt("Received register of new topic:");
        uint8_t msg_id_reg = data[5];
        uint8_t message_length_buf = data[0]-6;
        size_t j,t;
        char buff[MQTT_SN_MAX_TOPIC_LENGTH];

        short_topic = data[3];

        for (t = 0; t < message_length_buf; t++)
          buff[t] = data[t+6];
        buff[t] = '\0';

        for (j = 0; j < MAX_TOPIC_USED; j++)
          if (g_topic_bind[j].short_topic_id == 0xFF)
            break;

        char *s;
        s = (char *)malloc(strlen(buff)+1);
        strcpy(s,buff);
        g_topic_bind[j].short_topic_id = short_topic;
        g_topic_bind[j].subscribed = true;
        g_topic_bind[j].topic_name = s;

        debug_mqtt("Topic registered![%s]",g_topic_bind[j].topic_name);
        mqtt_sn_regack_send((uint16_t)msg_id_reg,(uint16_t)short_topic);
      break;
      case MQTT_SN_TYPE_WILLTOPICREQ:
        // debug_mqtt("Received um Packet WILL TOPIC REQ");
        if (mqtt_status == MQTTSN_WAITING_WILLTOPICREQ)
          process_post(&mqtt_sn_main,mqtt_event_will_topicreq,NULL);
      break;
      case MQTT_SN_TYPE_WILLMSGREQ:
        if (mqtt_status == MQTTSN_WAITING_WILLMSGREQ)
          process_post(&mqtt_sn_main,mqtt_event_will_messagereq,NULL);
      break;
      default:
        debug_mqtt("Received the message however not identified!");
      break;
    }
}

void mqtt_sn_udp_rec_cb(struct simple_udp_connection *c,
                            const uip_ipaddr_t *sender_addr,
                            uint16_t sender_port,
                            const uip_ipaddr_t *receiver_addr,
                            uint16_t receiver_port,
                            const uint8_t *data,
                            uint16_t datalen) {
  debug_udp("########## Received something from UDP!##########");
  mqtt_sn_recv_parser(data);
}

resp_con_t mqtt_sn_create_sck(mqtt_sn_con_t mqtt_sn_connection, char *topics[], size_t topic_len, mqtt_sn_cb_f cb_f){
  callback_mqtt = cb_f;
  /************************************ reconnection******************************/
  topics_len = topic_len;
  size_t t = 0;
  for (t=0; t < topic_len; t++){
    topics_reconnect[t] = topics[t];
  }
  /************************************ reconnection******************************/

  static uip_ipaddr_t broker_addr;
  static uint8_t con_udp_status = 0;

  g_mqtt_sn_con = mqtt_sn_connection;
  uip_ip6addr(&broker_addr, *g_mqtt_sn_con.ipv6_broker,
                            *(g_mqtt_sn_con.ipv6_broker+1),
                            *(g_mqtt_sn_con.ipv6_broker+2),
                            *(g_mqtt_sn_con.ipv6_broker+3),
                            *(g_mqtt_sn_con.ipv6_broker+4),
                            *(g_mqtt_sn_con.ipv6_broker+5),
                            *(g_mqtt_sn_con.ipv6_broker+6),
                            *(g_mqtt_sn_con.ipv6_broker+7));

  if (strlen(g_mqtt_sn_con.client_id) > 23){
    debug_mqtt("Cli. ID SIZE:%d > 23!",strlen(g_mqtt_sn_con.client_id));
    return FAIL_CON;
  }

  debug_mqtt("Address of broker IPv6: ");
  uip_debug_ipaddr_print(&broker_addr);
  debug_mqtt("Address of the port:%d ",g_mqtt_sn_con.udp_port);
  debug_mqtt("Client ID:%s/%d",g_mqtt_sn_con.client_id,strlen(g_mqtt_sn_con.client_id));


  if(!g_recon){
    con_udp_status = simple_udp_register(&g_mqtt_sn_con.udp_con,
                                          g_mqtt_sn_con.udp_port,
                                          &broker_addr,
                                          g_mqtt_sn_con.udp_port,
                                          mqtt_sn_udp_rec_cb);
    if(!con_udp_status)
      return FAIL_CON;
  }

  if (g_mqtt_sn_con.will_topic && g_mqtt_sn_con.will_message)
    g_will = true;

  /****************************************************************************/
  // Criando tarefa de [CONNECT]
  //
  // In the beginning we need to send a CONNECT to the broker MQTT-SN
  mqtt_sn_task_t connect_task;

  // debug_mqtt("Criando tarefa de CONNECT");
  connect_task.msg_type_q = MQTT_SN_TYPE_CONNECT;
  mqtt_sn_insert_queue(connect_task);
  /****************************************************************************/

  /****************************************************************************/
  // Resource implemented [LWT]
  // Check if the user want to use the will topic and the will message
  if (g_mqtt_sn_con.will_topic && g_mqtt_sn_con.will_message){
    mqtt_sn_task_t will_topic_task;
    will_topic_task.msg_type_q = MQTT_SN_TYPE_WILLTOPIC;
    mqtt_sn_insert_queue(will_topic_task);

    mqtt_sn_task_t will_message_task;
    will_message_task.msg_type_q = MQTT_SN_TYPE_WILLMSG;
    mqtt_sn_insert_queue(will_message_task);
  }

  /****************************************************************************/
  // Creating tasks of [REGISTER]
  //
  // To each of the defined topics in the main code. We init the process of filling the tasks in the service MQTT-SN
  // First before any process we register the topic sent by the user, then the broker will answer with the
  // correspondent SHORT TOPIC that'll use it.
  mqtt_sn_task_t topic_reg;

  size_t i;
  for(i = 0; i < topic_len; i++){
    if (g_mqtt_sn_con.will_topic && g_mqtt_sn_con.will_message)
      g_topic_bind[g_task_id-2].topic_name = topics_reconnect[i];
    else
      g_topic_bind[g_task_id].topic_name = topics_reconnect[i];
    topic_reg.msg_type_q = MQTT_SN_TYPE_REGISTER;
    if (!mqtt_sn_insert_queue(topic_reg)) break;
  }
  /****************************************************************************/

  process_post(&mqtt_sn_main, mqtt_event_run_task, NULL);

  return SUCCESS_CON;
}

void mqtt_sn_init(void){
  process_start(&mqtt_sn_main, NULL);

  // Allocation the number of event in the queue of events
  mqtt_event_connect         = process_alloc_event();
  mqtt_event_connack         = process_alloc_event();
  mqtt_event_register        = process_alloc_event();
  mqtt_event_regack          = process_alloc_event();
  mqtt_event_run_task        = process_alloc_event();
  mqtt_event_pub_qos_0       = process_alloc_event();
  mqtt_event_ping_timeout    = process_alloc_event();
  mqtt_event_suback          = process_alloc_event();
  mqtt_event_subscribe       = process_alloc_event();
  mqtt_event_connected       = process_alloc_event();
  mqtt_event_will_topicreq   = process_alloc_event();
  mqtt_event_will_messagereq = process_alloc_event();

  init_vectors();
}

void timeout_con(void *ptr){
  switch (mqtt_status) {
    case MQTTSN_WAITING_CONNACK:
      if (g_tries_send >= MQTT_SN_RETRY) {
        g_tries_send = 0;
        process_post(&mqtt_sn_main,mqtt_event_ping_timeout,NULL);
        debug_mqtt("Max. limit of  Packets CONNECT");
      }
      else{
        debug_mqtt("Expired the time of CONNECT");
        mqtt_sn_con_send();
        mqtt_status = MQTTSN_WAITING_CONNACK;
        ctimer_reset(&mqtt_time_connect);
        g_tries_send++;
      }
    break;
    case MQTTSN_WAITING_REGACK:
      if (g_tries_send >= MQTT_SN_RETRY) {
        g_tries_send = 0;
        process_post(&mqtt_sn_main,mqtt_event_ping_timeout,NULL);
        debug_mqtt("Max. limit of  Packets REGISTER");
      }
      else{
        debug_mqtt("Expired the time of REGISTER");
        mqtt_sn_reg_send();
        mqtt_status = MQTTSN_WAITING_REGACK;
        ctimer_reset(&mqtt_time_register);
        g_tries_send++;
      }
    break;
    case MQTTSN_WAITING_SUBACK:
      if (g_tries_send >= MQTT_SN_RETRY) {
        g_tries_send = 0;
        process_post(&mqtt_sn_main,mqtt_event_ping_timeout,NULL);
        debug_mqtt("Max. limit of  Packets SUBSCRIBE");
      }
      else{
        debug_mqtt("Expired the time of SUBSCRIBE");
        mqtt_sn_sub_send(g_topic_bind[mqtt_queue_first->data.short_topic].topic_name, mqtt_queue_first->data.qos_level);
        mqtt_status = MQTTSN_WAITING_SUBACK;
        ctimer_reset(&mqtt_time_subscribe);
        g_tries_send++;
      }
    break;
    case MQTTSN_WAITING_WILLTOPICREQ:
      if (g_tries_send >= MQTT_SN_RETRY) {
        g_tries_send = 0;
        process_post(&mqtt_sn_main,mqtt_event_ping_timeout,NULL);
        debug_mqtt("Max. limit of  Packets CONNECT for WILL TOPIC");
      }
      else{
        debug_mqtt("Expired the time of CONNECT for WILL TOPIC");
        mqtt_sn_con_send();
        mqtt_status = MQTTSN_WAITING_WILLTOPICREQ;
        ctimer_reset(&mqtt_time_connect);
        g_tries_send++;
      }
    break;
    case MQTTSN_CONNECTED:
      //process_post(&mqtt_sn_main, mqtt_event_run_task, NULL);
    break;
    default:
      debug_mqtt("Expired the time of unknow state");
    break;
  }
}

void timeout_ping_mqtt(void *ptr){
  //debug_mqtt("\nTentativas PING:%d",g_tries_ping);
  if (g_ping_flag_resp) {
    g_ping_flag_resp = false;
    g_tries_ping = 0;
    //debug_mqtt("Sending PING REQUEST");
    mqtt_sn_ping_send();
  }
  else{
    if (g_tries_ping >= MQTT_SN_RETRY_PING) {
      g_tries_ping = 0;
      ctimer_stop(&mqtt_time_ping);
      if (mqtt_status != MQTTSN_DISCONNECTED)
        process_post(&mqtt_sn_main,mqtt_event_ping_timeout,NULL);
      debug_mqtt("Limit of tries of PING RESPONSE");
    }
    else{
      debug_mqtt("Incrementing PING");
      mqtt_sn_ping_send();
      g_tries_ping++;
    }
  }
  ctimer_reset(&mqtt_time_ping);
}

PROCESS_THREAD(mqtt_sn_main, ev, data){
  PROCESS_BEGIN();

  debug_mqtt("Beginning of the MQTT-SN process");

  while(1) {
      PROCESS_WAIT_EVENT();
      /*************************** CONNECT MQTT-SN ****************************/
      if (ev == mqtt_event_connect &&
          mqtt_status == MQTTSN_DISCONNECTED){
        mqtt_sn_con_send();
        if (g_mqtt_sn_con.will_topic && g_mqtt_sn_con.will_message)
          mqtt_status = MQTTSN_WAITING_WILLTOPICREQ;
        else
          mqtt_status = MQTTSN_WAITING_CONNACK;
        ctimer_set(&mqtt_time_connect, MQTT_SN_TIMEOUT_CONNECT, timeout_con, NULL);
        g_tries_send = 0;
      }
      else if(ev == mqtt_event_connack){
        mqtt_status = MQTTSN_CONNECTED;
        debug_mqtt("CONNECTED to the broker MQTT-SN");
        ctimer_stop(&mqtt_time_connect);
        mqtt_sn_delete_queue(); // Delete req. if CONNECT if we're CONNECTED;
        ctimer_set(&mqtt_time_ping, g_mqtt_sn_con.keep_alive*CLOCK_SECOND , timeout_ping_mqtt, NULL);
        process_post(&mqtt_sn_main, mqtt_event_run_task, NULL);
      }

      /************************** WILL TOPIC MQTT-SN **************************/
      if(ev == mqtt_event_will_topicreq){
        mqtt_status = MQTTSN_WAITING_WILLMSGREQ;
        mqtt_sn_will_topic_send();
        mqtt_sn_delete_queue();
      }

      /************************** WILL MESSAGE MQTT-SN ************************/
      else if(ev == mqtt_event_will_messagereq){
        mqtt_sn_delete_queue();
        mqtt_sn_will_message_send();
        mqtt_status = MQTTSN_WAITING_CONNACK;
      }

      /*************************** REGISTER MQTT-SN ***************************/
      else if(ev == mqtt_event_register &&
              mqtt_queue_first->data.msg_type_q == MQTT_SN_TYPE_REGISTER){
        mqtt_sn_reg_send();
        mqtt_status = MQTTSN_WAITING_REGACK;
        ctimer_set(&mqtt_time_register, MQTT_SN_TIMEOUT, timeout_con, NULL);
        g_tries_send = 0;
      }
      else if(ev == mqtt_event_regack &&
              mqtt_queue_first->data.msg_type_q == MQTT_SN_TYPE_REGISTER){
        mqtt_sn_delete_queue(); // Deleta requisição de REGISTER
        ctimer_stop(&mqtt_time_register);
        debug_mqtt("Topic registered in the broker");

        if (!mqtt_sn_check_empty())
          process_post(&mqtt_sn_main, mqtt_event_run_task, NULL); // Gera evento de processo de tasks
        else{
          mqtt_status = MQTTSN_TOPIC_REGISTERED;
          process_post(&mqtt_sn_main, mqtt_event_connected, NULL); // Gera evento de processo de tasks
        }
      }

      /*************************** RUN TASKS MQTT-SN **************************/
      else if(ev == mqtt_event_run_task){
        char *teste;
        parse_mqtt_type_string(mqtt_queue_first->data.msg_type_q,&teste);
        debug_task("Task to run:%s",teste);
        switch (mqtt_queue_first->data.msg_type_q) {
          case MQTT_SN_TYPE_CONNECT:
            process_post(&mqtt_sn_main, mqtt_event_connect, NULL);
          break;
          case MQTT_SN_TYPE_PUBLISH:
            process_post(&mqtt_sn_main, mqtt_event_pub_qos_0, NULL);
          break;
          case MQTT_SN_TYPE_SUBSCRIBE:
            process_post(&mqtt_sn_main,mqtt_event_subscribe,NULL);
          break;
          case MQTT_SN_TYPE_REGISTER:
            process_post(&mqtt_sn_main, mqtt_event_register, NULL);
          break;
          case MQTT_SN_TYPE_SUB_WILDCARD:
            mqtt_sn_sub_send_wildcard(topic_temp_wildcard, mqtt_queue_first->data.qos_level);
          break;
          case MQTT_SN_TYPE_WILLTOPIC:
          break;
          case MQTT_SN_TYPE_WILLMSG:
          break;
          default:
            mqtt_status = MQTTSN_TOPIC_REGISTERED;
            debug_task("None tasks to process!");
          break;
        }
      }

      /********************** PUBLISH QoS 0 - MQTT-SN *************************/
      else if(ev == mqtt_event_pub_qos_0 &&
              mqtt_queue_first->data.msg_type_q == MQTT_SN_TYPE_PUBLISH){
        // This event just occurs when we doesn't know the topic to register and we need to register it
        // in the oppose case the API send to the broker without to create the task.
        debug_mqtt("Primeira publicacao de novo topico");
        // We get the complete name do the topic for the first publish
        size_t j = 0;
        for (j=0; j < MAX_TOPIC_USED; j++)
          if (g_topic_bind[j].short_topic_id == 0xFF)
            break;
        mqtt_sn_pub_send(g_topic_bind[j-1].topic_name,
                         g_message_bind,
                         mqtt_queue_first->data.retain,
                         mqtt_queue_first->data.qos_level);
        mqtt_sn_delete_queue(); // Delete the req. of PUBLISH
        if (!mqtt_sn_check_empty())
          process_post(&mqtt_sn_main, mqtt_event_run_task, NULL); // Init. another tasks if is not empty
      }

      /*************************** SUBSCRIBE MQTT-SN **************************/
      else if(ev == mqtt_event_subscribe &&
              mqtt_queue_first->data.msg_type_q == MQTT_SN_TYPE_SUBSCRIBE){
        mqtt_sn_sub_send(g_topic_bind[mqtt_queue_first->data.short_topic].topic_name, mqtt_queue_first->data.qos_level);
        mqtt_status = MQTTSN_WAITING_SUBACK;
        ctimer_set(&mqtt_time_subscribe, 3*MQTT_SN_TIMEOUT, timeout_con, NULL);
        g_tries_send = 0;
      }
      else if(ev == mqtt_event_suback &&
              mqtt_queue_first->data.msg_type_q == MQTT_SN_TYPE_SUBSCRIBE){
        mqtt_sn_delete_queue(); // Delete req. of SUBSCRIBE
        ctimer_stop(&mqtt_time_subscribe);
        debug_mqtt("Topico inscrito no broker");
        if (!mqtt_sn_check_empty())
          process_post(&mqtt_sn_main, mqtt_event_run_task, NULL); // Generates the event of process tasks
        else
          mqtt_status = MQTTSN_TOPIC_REGISTERED; // Release other publishings and other operations
      }

      /********************** PING REQUEST - MQTT-SN **************************/
      else if(ev == mqtt_event_ping_timeout){
        ctimer_stop(&mqtt_time_connect);
        ctimer_stop(&mqtt_time_register);
        ctimer_stop(&mqtt_time_ping);
        ctimer_stop(&mqtt_time_subscribe);

        mqtt_status = MQTTSN_DISCONNECTED;
        debug_mqtt("DISCONNECTED broker");
        #ifdef MQTT_SN_AUTO_RECONNECT
          g_recon = true;
          init_vectors();
          mqtt_sn_create_sck(g_mqtt_sn_con, topics_reconnect, topics_len, callback_mqtt);
        #endif
      }
  }
  PROCESS_END();
}
