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
 * @license Este projeto está sendo liberado pela licença APACHE 2.0.
 * @file mqtt_sn.c
 * @author Ânderson Ignácio da Silva
 * @date 19 Ago 2016
 * @brief Arquivo principal do código fonte do porte do MQTT-SN para o Contiki
 * @see http://www.aignacio.com
 * @todo Passar para a função de recepção de publicações de callback um ponteiro
         para o dado e o comprimento ao invés de usar um for

 [Apontamento n-1]:
 Descoberta uma característica do contiki, o que ocorre é que se você utiliza
 algum  temporizador de evento (etimer) e ele expira, ele gera um evento o que
 e esperado, porém se você tiver testando diversas condições de forma sequencial
 1) else if(ev == etimer_expired(....))
 2) else if(ev == mqtt_event_regack)
 ...
 MESMO após ter gerado o evento de "expiração", caso a condição 1) esteja antes
 da 2), e a 2) que gerou um evento, a 1) será executada anteriormente porque
 compreendesse que o "timer continua expirado".

 [Apontamento n-2]:
 Como nesta API utiliza-se malloc necessitamos da função do tipo POSIX chamadas
 "sbrk" a qual utiliza de chamadas de sistema para realizar a operação de aloca
 ção de memória.Logo, devemos incluir no arquivo principal o arquivo syscalls.c
 (arquivo disponibilizado pelo contiki - lib/newlib/syscalls.c) e também  deve-
 mos incluir as variáveis _heap e _eheap no linker script (cc26xx.ld) para a
 função sbrk saber onde começa e termina a zona heap de memória do mcu.

 Incluir no linker:
 ...
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
#include "lib/newlib/syscalls.c" //Utilizado quando se usa malloc
#endif

static struct ctimer              mqtt_time_connect;       // Estrutura de temporização para envio de CONNECT
static struct ctimer              mqtt_time_register;      // Estrutura de temporização para envio de REGISTER
static struct ctimer              mqtt_time_ping;          // Estrutura de temporização para envio de PING
static struct ctimer              mqtt_time_subscribe;     // Estrutura de temporização para envio de SUBSCRIBE

static process_event_t            mqtt_event_connect;          // Evento de req CONNECT  [nó --> broker]
static process_event_t            mqtt_event_connack;          // Evento de req CONNACK  [broker --> nó]
static process_event_t            mqtt_event_register;         // Evento de req REGISTER [nó --> broker]
static process_event_t            mqtt_event_regack;           // Evento de req REGACK   [broker --> nó]
static process_event_t            mqtt_event_pub_qos_0;        // Evento de req PUBLISH - QoS - 0 [nó --> broker]
static process_event_t            mqtt_event_subscribe;        // Evento de req SUBSCRIBE  [nó --> broker]
static process_event_t            mqtt_event_suback;           // Evento de req SUBACK  [broker --> nó]
static process_event_t            mqtt_event_run_task;         // Evento de req qualquer, seja PUBLISH ou SUBSCRIBE [nó --> broker]
static process_event_t            mqtt_event_ping_timeout;     // Evento de req. PING REQUEST em excesso de não resposta do broker [broker <-> nó]
static process_event_t            mqtt_event_connected;        // Evento de req. PING REQUEST em excesso de não resposta do broker [broker <-> nó]
static process_event_t            mqtt_event_will_topicreq;    // Evento de req. WILL TOPIC REQUEST [broker <-> nó]
static process_event_t            mqtt_event_will_messagereq;  // Evento de req. WILL MESSAGE REQUEST [broker <-> nó]

// Comentado por enquanto já que QoS - 0 não envia PUBACK
// static process_event_t            mqtt_event_puback;     // Evento de req PUBACK   [broker --> nó]
static bool                       g_recon = false;                   // Identificador de reconexão evitando dupla conexão UDP aberta
static bool                       g_will = false;                    // Identificador de utilização de LWT
static bool                       g_ping_flag_resp = true;           // Identificador de resposta ao PING REQUEST
static char                       *g_message_bind;                   // Buffer temporário para o envio de mensagens do tipo publicação no caso de tarefas
static char                       *topic_temp_wildcard;              // Buffer temporário para o armazenamento do buffer de inscrição wildcard
static uint8_t                    g_tries_send = 0;                  // Identificador de tentativas de envio
static uint8_t                    g_tries_ping = 0;                  // Identificador de tentativas de envio de PING REQUEST
static uint8_t                    g_task_id = 0;                     // Identificador unitário de tarefa incremental
static short_topics_t             g_topic_bind[MAX_TOPIC_USED];      // Vetor que armazena a relação nome do tópico com short topic id
static mqtt_sn_con_t              g_mqtt_sn_con;                     // Estrutura principal da conexão MQTT
static mqtt_sn_status_t           mqtt_status = MQTTSN_DISCONNECTED; // ASM principal do MQTT-SN
static char                       *topics_reconnect[MAX_TOPIC_USED]; // Vetor de tópicos [reconexão]
static uint16_t                   topics_len;                        // Comprimento total de tópicos fornecidos pelo usuário [reconexão]
static mqtt_sn_cb_f               callback_mqtt;

PROCESS(mqtt_sn_main, "[MQTT-SN] Processo inicial");

/*********************** FUNÇÕES AUXILIARES MQTT-SN ***************************/
bool unlock_tasks(void) {
  if (mqtt_status == MQTTSN_TOPIC_REGISTERED)
    return true;
  return false;
}

void init_sub(void *ptr){
  debug_mqtt("INICIANDO SUBSCRIBE");
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
      *type_string = "Estado nao definido nas opcoes";
    break;
  }
}

char* mqtt_sn_check_status_string(void){
  switch (mqtt_status) {
    case MQTTSN_DISCONNECTED:
      return "DESCONECTADO";
    break;
    case MQTTSN_WAITING_CONNACK:
      return "AGUARDANDO CONNACK";
    break;
    case MQTTSN_WAITING_REGACK:
      return "AGUARDANDO REGACK";
    break;
    case MQTTSN_CONNECTED:
      return "#### CONECTADO ####";
    break;
    case MQTTSN_TOPIC_REGISTERED:
      return "TOPICOS REGISTRADOS";
    break;
    case MQTTSN_WAITING_WILLTOPICREQ:
      return "AGUARDANDO WILL TOPIC";
    break;
    case MQTTSN_WAITING_WILLMSGREQ:
      return "AGUARDANDO WILL MESSAGE";
    break;
    default:
      return "ESTADO NAO DESCRITO";
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
   debug_task("ERRO AO ADICIONAR NA FILA");

  return SUCCESS_CON;
}

resp_con_t mqtt_sn_sub(char *topic, uint8_t qos){
  // Caso haja tópicos para registrar, não habilita a inscrição
  // evitando que prejudique alguma transação, ou seja, tasks
  // tem prioridade sobre inscrições diretas
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
      if(strcmp(g_topic_bind[i].topic_name,topic) == 0) // Tópico novo ou existe?
        break;

    subscribe_task.msg_type_q      = MQTT_SN_TYPE_SUBSCRIBE;
    subscribe_task.qos_level       = qos;
    subscribe_task.short_topic     = i;

    // Comentadas as duas linhas abaixo porque consideraremos que o usuário irá registrar os
    // topicos no começo do programa não sendo necessário gerar o evento de run_task
    //if (mqtt_sn_check_empty())
    //  ctimer_set(&mqtt_time_subscribe, 3*MQTT_SN_TIMEOUT, init_sub, NULL);

    if (!mqtt_sn_insert_queue(subscribe_task))
     debug_task("ERRO AO ADICIONAR NA FILA");

    return SUCCESS_CON;
  }
  else
    return FAIL_CON;

}

resp_con_t mqtt_sn_pub(char *topic,char *message, bool retain_flag, uint8_t qos){
  // Caso haja tópicos para registrar, não habilita a publicação
  // evitando que prejudique alguma transação, ou seja, tasks
  // tem prioridade sobre publicações diretas
  if (!unlock_tasks())
    return FAIL_CON;

  // Analisamos o buffer de tópicos registrados para ver se já foi registrado o tópico
  if (!verf_register(topic))
    return FAIL_CON;

  mqtt_sn_pub_send(topic,message,retain_flag,qos);
  return SUCCESS_CON;
}

resp_con_t verf_hist_sub(char *topic){
  size_t i;
  //
  // for (i=0; i < MAX_TOPIC_USED; i++)
  //   if(strcmp(g_topic_bind[i].topic_name,topic) == 0) // Tópico novo ou existe?
  //     break;
  //
  // if (g_topic_bind[i].short_topic_id == 0xFF) {
  //   debug_mqtt("Topico nao registrado!")
  //   return FAIL_CON;
  // }

  for (i=0; i < MAX_TOPIC_USED; i++)
    if(strcmp(g_topic_bind[i].topic_name,topic) == 0)
      break;

  if (g_topic_bind[i].subscribed == 0x01){  // Na fila para inscrever? 0x01?
    debug_mqtt("Inscricao do topico em andamento:[%s]",g_topic_bind[i].topic_name);
    return FAIL_CON;
  }
  else if (g_topic_bind[i].subscribed == 0x02) {  // Já inscrito? 0x02?
    debug_mqtt("Topico inscrito:[%s]",g_topic_bind[i].topic_name);
    return FAIL_CON;
  }
  else if (g_topic_bind[i].subscribed == 0x00){  // Caso g_topic_bind[i].subscribed == 0x00 vamos preparar o topico para ser inscrito
    g_topic_bind[i].subscribed = 0x01;
    debug_mqtt("Preparando para inscricao:[%s]",g_topic_bind[i].topic_name);
    return SUCCESS_CON;
  }
  else{
    debug_mqtt("Topico com valor estranho no SUBSCRIBED:%d",g_topic_bind[i].subscribed);
    return FAIL_CON;
  }

}

resp_con_t verf_register(char *topic){
  size_t i;
  for (i=0; i < MAX_TOPIC_USED; i++)
    if(strcmp(g_topic_bind[i].topic_name,topic) == 0)  // Tópico novo ou existe?
      return SUCCESS_CON;

  debug_mqtt("Topico nao registrado!");
  return FAIL_CON;
}

void print_g_topics(void){
  size_t i;
  debug_mqtt("Vetor de topicos");
  for(i = 0 ; g_topic_bind[i].short_topic_id != 0xFF; i++) {
    debug_mqtt("[i=%d][%d][%s]",i,g_topic_bind[i].short_topic_id,g_topic_bind[i].topic_name);
  }
}

void init_vectors(void){
  debug_mqtt("Inicializando vetores...");
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

/******************** FUNÇÕES DE ENVIO DE PACOTES MQTT-SN *********************/
resp_con_t mqtt_sn_will_topic_send(void){
  willtopic_packet_t packet;

  size_t topic_name_len = strlen(g_mqtt_sn_con.will_topic);

  if (topic_name_len > MQTT_SN_MAX_TOPIC_LENGTH) {
    debug_mqtt("Erro: Nome do topico WILL excede o limite maximo");
    return FAIL_CON;
  }

  packet.flags = MQTT_SN_FLAG_RETAIN;

  packet.type = MQTT_SN_TYPE_WILLTOPIC;

  strncpy(packet.will_topic, g_mqtt_sn_con.will_topic, topic_name_len);
  packet.length = 0x03 + topic_name_len;
  packet.will_topic[topic_name_len] = '\0';

  debug_mqtt("Enviando o pacote @WILL TOPIC");
  simple_udp_send(&g_mqtt_sn_con.udp_con,&packet, packet.length);

  return SUCCESS_CON;
}

resp_con_t mqtt_sn_will_message_send(void){
  willmessage_packet_t packet;

  size_t message_name_len = strlen(g_mqtt_sn_con.will_message);

  if (message_name_len > MQTT_SN_MAX_TOPIC_LENGTH) {
    debug_mqtt("Erro: Nome da mensagem WILL excede o limite maximo");
    return FAIL_CON;
  }

  packet.type = MQTT_SN_TYPE_WILLMSG;

  strncpy(packet.will_message, g_mqtt_sn_con.will_message, message_name_len);
  packet.length = 0x02 + message_name_len;
  packet.will_message[message_name_len] = '\0';

  debug_mqtt("Enviando o pacote @WILL MESSAGE");
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
  //debug_mqtt("Enviando @PINGREQ");
  simple_udp_send(&g_mqtt_sn_con.udp_con,&ping_request, ping_request.length);
}

resp_con_t mqtt_sn_con_send(void){
  connect_packet_t packet;

  // Criação do pacote CONNECT
  packet.type = MQTT_SN_TYPE_CONNECT;
  packet.flags = MQTT_SN_FLAG_CLEAN;
  if (g_will)
    packet.flags += MQTT_SN_FLAG_WILL;
  packet.protocol_id = MQTT_SN_PROTOCOL_ID;
  packet.duration = uip_htons(g_mqtt_sn_con.keep_alive); //Realiza a conversão para network byte order

  strncpy(packet.client_id, g_mqtt_sn_con.client_id, strlen(g_mqtt_sn_con.client_id));
  packet.client_id[strlen(g_mqtt_sn_con.client_id)] = '\0';
  packet.length = 0x06 + strlen(packet.client_id);

  // debug_mqtt("CLIENT_ID:%s, Tamanho:%d",packet.client_id,strlen(packet.client_id));
  debug_mqtt("Enviando o pacote @CONNECT ");
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
  //size_t topic_name_len = strlen(mqtt_queue_first->data.long_topic); //Pega o primeiro da fila aguardando
  size_t topic_name_len = strlen(g_topic_bind[task_id].topic_name);

  if (topic_name_len > MQTT_SN_MAX_TOPIC_LENGTH) {
    debug_mqtt("Erro: Nome do topico excede o limite maximo");
    return FAIL_CON;
  }

  if (mqtt_queue_first->data.msg_type_q != MQTT_SN_TYPE_REGISTER) {
    debug_mqtt("Erro: Pacote a processar nao e do tipo REGISTER");
    return FAIL_CON;
  }

  packet.type = MQTT_SN_TYPE_REGISTER;
  packet.topic_id = 0x0000;

  // Quando o broker responder com o short topic ID,
  // ele utilizará como message id, o identificador único da task na
  // queue de serviços do MQTT-SN, logo se torna fácil saber como montar
  // a relação (short_topic/long_topic) no vetor global g_topic_bind[]
  packet.message_id = uip_htons(task_id);

  strncpy(packet.topic_name, g_topic_bind[task_id].topic_name, topic_name_len);
  packet.length = 0x06 + topic_name_len;
  packet.topic_name[topic_name_len] = '\0';

  debug_mqtt("Topico a registrar:%s [%d][MSG_ID:%d]",packet.topic_name,strlen(packet.topic_name),(int)mqtt_queue_first->data.id_task);
  debug_mqtt("Enviando o pacote @REGISTER");
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

  debug_mqtt("Enviando o pacote @REGACK");
  simple_udp_send(&g_mqtt_sn_con.udp_con,&packet, packet.length);
  return SUCCESS_CON;
}

resp_con_t mqtt_sn_pub_send(char *topic,char *message, bool retain_flag, uint8_t qos){
  publish_packet_t packet;
  uint16_t stopic = 0x0000;
  uint8_t data_len = strlen(message);

  // if (mqtt_queue_first->data.msg_type_q != MQTT_SN_TYPE_PUBLISH) {
  //   debug_mqtt("Erro: Pacote a processar nao e do tipo PUBLISH");
  //   return FAIL_CON;
  // }
  size_t i = 0;
  for (i=0; i < MAX_TOPIC_USED; i++)
    if (strcmp(g_topic_bind[i].topic_name,topic) == 0) {
      stopic = g_topic_bind[i].short_topic_id;
      break;
    }

  if (data_len > sizeof(packet.data)) {
      printf("Erro: Payload e muito grande!\n");
      return FAIL_CON;
  }

  packet.type  = MQTT_SN_TYPE_PUBLISH;
  packet.flags = 0x00;

  if (retain_flag)
    packet.flags += MQTT_SN_FLAG_RETAIN;

  packet.flags += mqtt_sn_get_qos_flag(qos);

  // Segundo a especificação:
  // TopicIdType: indicates whether the field TopicId or TopicName included in this message contains a normal
  // topic id (set to “0b00”), a pre-defined topic id (set to “0b01”), or a short topic name (set to “0b10”). The
  // value “0b11” is reserved. Refer to sections 3 and 6.7 for the definition of the various types of topic ids.
  packet.flags += MQTT_SN_TOPIC_TYPE_NORMAL; //Utiliza-se o topic id já registrado

  packet.topic_id = uip_htons(stopic);
  packet.message_id = uip_htons(0x00); //Relevante somente se QoS > 0
  strncpy(packet.data, message, data_len+1);
  //
  //  Pacote PUBLISH
  //  _________________ ______________________ ___________ ________________ ______________ ________________
  // | Comprimento - 0 | Tipo de mensagem - 1 | Flags - 2 | Topic ID - 3,4 | Msg ID - 5,6 | Dado - 7,n ....|
  // |_________________|______________________|___________ ________________|______________|________________|
  //
  packet.length = 0x07 + (data_len+1);

  debug_mqtt("Enviando o pacote @PUBLISH");
  // debug_mqtt("Enviando o pacote @PUBLISH - Task:[%d]",(int)mqtt_queue_first->data.id_task);
  simple_udp_send(&g_mqtt_sn_con.udp_con,&packet, packet.length);
  return SUCCESS_CON;
}

resp_con_t mqtt_sn_sub_send(char *topic, uint8_t qos){
  subscribe_packet_t packet;
  uint16_t stopic = 0x0000;

  // if (mqtt_queue_first->data.msg_type_q != MQTT_SN_TYPE_PUBLISH) {
  //   debug_mqtt("Erro: Pacote a processar nao e do tipo PUBLISH");
  //   return FAIL_CON;
  // }
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
  packet.flags += MQTT_SN_TOPIC_TYPE_PREDEFINED; //Utiliza-se o topic id já registrado
  packet.length = 0x07;
  //
  //  Pacote SUBSCRIBE
  //  _________________ ______________________ ___________ ________________ _____________________________________
  // | Comprimento - 0 | Tipo de mensagem - 1 | Flags - 2 | Msg ID - 3,4  | Topic ID - 5,6 ou Topic name 5,n ....|
  // |_________________|______________________|___________|_______________|______________________________________|
  //
  debug_mqtt("Enviando o pacote @SUBSCRIBE");
  simple_udp_send(&g_mqtt_sn_con.udp_con,&packet, packet.length);
  return SUCCESS_CON;
}

resp_con_t mqtt_sn_sub_send_wildcard(char *topic, uint8_t qos){
  subscribe_wildcard_packet_t packet;

  // Analisamos o buffer de tópicos registrados para ver se já foi registrado o tópico
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
  //  Pacote SUBSCRIBE
  //  _________________ ______________________ ___________ ________________ _____________________________________
  // | Comprimento - 0 | Tipo de mensagem - 1 | Flags - 2 | Msg ID - 3,4  | Topic ID - 5,6 ou Topic name 5,n ....|
  // |_________________|______________________|___________|_______________|______________________________________|
  //
  debug_mqtt("Enviando o pacote @SUBSCRIBE(Wildcard)");
  simple_udp_send(&g_mqtt_sn_con.udp_con,&packet, packet.length);
  return SUCCESS_CON;
}

resp_con_t mqtt_sn_disconnect(uint16_t duration){
  disconnect_packet_t packet;

  packet.msg_type = MQTT_SN_TYPE_DISCONNECT;
  packet.duration = uip_htons(duration);
  packet.length = 0x04;
  debug_mqtt("Desconectando do broker...");

  simple_udp_send(&g_mqtt_sn_con.udp_con,&packet, packet.length);
  return SUCCESS_CON;
}

/************************** FUNÇÕES DE FILA MQTT-SN ***************************/
resp_con_t mqtt_sn_insert_queue(mqtt_sn_task_t new){
  struct node *temp,*temp2;

  temp2 = mqtt_queue_first;
  int cnt = 0;
  while (temp2) {
      temp2 = temp2->link;
      cnt++;
  }

  //Limita o número máximo de tarefas alocadas na fila
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
  debug_task("Task adicionada:[%2.0d][%s]",(int)temp->data.id_task, task_type);
  return SUCCESS_CON;
}

void mqtt_sn_delete_queue(void){
  struct node *temp;
  char *task_type;

  temp = mqtt_queue_first;
  if (mqtt_queue_first->link == NULL) {
      g_task_id = 0;
      parse_mqtt_type_string(mqtt_queue_first->data.msg_type_q,&task_type);
      debug_task("Task removida:[%2.0d][%s]",(int)mqtt_queue_first->data.id_task,task_type);
      debug_task("Task info: Fila vazia");
      mqtt_queue_first = mqtt_queue_last = NULL;
  }
  else {
      g_task_id--;
      parse_mqtt_type_string(mqtt_queue_first->data.msg_type_q,&task_type);
      debug_task("Task removida:[%2.0d][%s]",(int)mqtt_queue_first->data.id_task,task_type);
      mqtt_queue_first = mqtt_queue_first->link;
      free(temp);
  }
}

void mqtt_sn_check_queue(void){
  int cnt = 0;
  struct node *temp;
  char *task_type;

  temp = mqtt_queue_first;

  debug_task("VALOR DO GLOBAL ID g_task_id:%d",g_task_id);

  debug_task("FILA:");
  while (temp) {
      parse_mqtt_type_string(temp->data.msg_type_q,&task_type);
      debug_task("[%2.0d][%s][%d]",(int)temp->data.id_task, task_type,temp->data.short_topic);
      temp = temp->link;
      cnt++;
  }
  debug_task("Tamanho da fila:[%d]", cnt);
}

bool mqtt_sn_check_empty(void){
  if (mqtt_queue_first  ==  NULL)
    return true;
  else
    return false;
}

/************** FUNÇÕES DE GERENCIMENTO DE CONEXÃO MQTT-SN ********************/
void mqtt_sn_recv_parser(const uint8_t *data){
    uint8_t msg_type = data[1],
            return_code = 0xFF,
            short_topic;
    size_t i = 0;
    // Como o MsgType não se altera de posição, testamos primeiro ele antes do
    // returning code, já que este pode variar
    switch (msg_type) {
      case MQTT_SN_TYPE_CONNACK:
        return_code = data[2]; //No caso do CONNACK - RC[2]
        if (mqtt_sn_check_rc(return_code)){
          if (mqtt_status == MQTTSN_WAITING_CONNACK)
            process_post(&mqtt_sn_main, mqtt_event_connack, NULL);
          else
            debug_mqtt("Recebido CONNAC sem requisicao!");
        }
      break;
      case MQTT_SN_TYPE_REGACK:
        return_code = data[6]; //No caso do REGACK - RC[6]
        short_topic = data[3];
        // Na verdade os bytes de short topic são o [2] e [3], porém
        // só estamos usa-se o [3] porque não consideramos mais do que
        // 15 tópicos
        /// @todo Rever o short topic para adequar bytes [2][3] juntos..
        if (mqtt_sn_check_rc(return_code)){
          for (i = 0;i < MAX_TOPIC_USED; i++) { //Compara o byte menor do MSG ID para atribuir o short topic a requisição REGISTER correta
            if (i == data[5]){
              g_topic_bind[i].short_topic_id = short_topic;
            }
          }
          if (mqtt_queue_first->data.msg_type_q == MQTT_SN_TYPE_REGISTER &&
              mqtt_status == MQTTSN_WAITING_REGACK)
            process_post(&mqtt_sn_main, mqtt_event_regack, NULL);
          else
            debug_mqtt("Recebido REGACK sem requisicao!");
        }
      break;
      case MQTT_SN_TYPE_PUBACK:
        // return_code = data[6]; //No caso do PUBACK - RC[6]
        // short_topic = data[3];
        // // Na verdade os bytes de short topic são o [2] e [3], porém
        // // só estamos usa-se o [3] porque não consideramos mais do que
        // // 15 tópicos
        // /// @todo Rever o short topic para adequar bytes [2][3] juntos
        //
        // /// @TODO Implementar verificação de message ID e short topic ID para melhorar a confiança do recebimento
        // if (mqtt_sn_check_rc(return_code))
        //   if (mqtt_queue_first->data.msg_type_q == MQTT_SN_TYPE_PUBLISH)
        //     process_post(&mqtt_sn_main, mqtt_event_puback, NULL);
      break;
      case MQTT_SN_TYPE_SUBACK:
        return_code = data[7]; //No caso do SUBACK - RC[7]
        short_topic = data[4];
        // Na verdade os bytes de short topic são o [2] e [3], porém
        // só estamos usa-se o [3] porque não consideramos mais do que
        // 15 tópicos
        /// @todo Rever o short topic para adequar bytes [2][3] juntos...
        debug_mqtt("Recebido SUBACK");

        if (mqtt_sn_check_rc(return_code))
          if (short_topic != 0x00) {
            debug_mqtt("Reconhecimento de inscricao:[%s]",g_topic_bind[short_topic].topic_name);
            g_topic_bind[short_topic].subscribed = 0x02;
            if (mqtt_queue_first->data.msg_type_q == MQTT_SN_TYPE_SUBSCRIBE &&
                mqtt_status == MQTTSN_WAITING_SUBACK)
              process_post(&mqtt_sn_main, mqtt_event_suback, NULL);
            else
              debug_mqtt("Recebido SUBACK sem requisicao!");
          }
          else{
            debug_mqtt("Recebido SUBACK de WILDCARD");
            if (mqtt_queue_first->data.msg_type_q == MQTT_SN_TYPE_SUB_WILDCARD){
              mqtt_status = MQTTSN_TOPIC_REGISTERED;
              mqtt_sn_delete_queue();
            }
          }
        else
          debug_mqtt("Erro: Codigo de retorno invalido");
      break;
      case MQTT_SN_TYPE_PINGRESP:
        g_ping_flag_resp = true;
        //debug_mqtt("Ping respondido");
      break;
      case MQTT_SN_TYPE_PINGREQ:
        mqtt_sn_ping_send();
      break;
      case MQTT_SN_TYPE_PUBLISH:
        debug_mqtt("Recebida publicacao:");
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
        debug_mqtt("Recebido registro de topico novo:");
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

        // Apesar de a variável ser local (buff) precisamos alocar dinamicamente
        // memória para o ponteiro s para que consigamos fornecer um novo endereço
        // de memória para a estrutura g_topic_bind...
        char *s;
        s = (char *)malloc(strlen(buff)+1);
        strcpy(s,buff);
        g_topic_bind[j].short_topic_id = short_topic;
        g_topic_bind[j].subscribed = true;
        g_topic_bind[j].topic_name = s;

        debug_mqtt("Topico registrado![%s]",g_topic_bind[j].topic_name);
        mqtt_sn_regack_send((uint16_t)msg_id_reg,(uint16_t)short_topic);
      break;
      case MQTT_SN_TYPE_WILLTOPICREQ:
        // debug_mqtt("Recebido um pacote WILL TOPIC REQ");
        if (mqtt_status == MQTTSN_WAITING_WILLTOPICREQ)
          process_post(&mqtt_sn_main,mqtt_event_will_topicreq,NULL);
      break;
      case MQTT_SN_TYPE_WILLMSGREQ:
        if (mqtt_status == MQTTSN_WAITING_WILLMSGREQ)
          process_post(&mqtt_sn_main,mqtt_event_will_messagereq,NULL);
      break;
      default:
        debug_mqtt("Recebida mensagem porem nao identificada!");
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
  debug_udp("##########RECEBIDO ALGO VIA UDP!##########");
  mqtt_sn_recv_parser(data);
}

resp_con_t mqtt_sn_create_sck(mqtt_sn_con_t mqtt_sn_connection, char *topics[], size_t topic_len, mqtt_sn_cb_f cb_f){
  callback_mqtt = cb_f;
  /************************************ RECONEXÃO******************************/
  topics_len = topic_len;
  size_t t = 0;
  for (t=0; t < topic_len; t++){
    topics_reconnect[t] = topics[t];
    // debug_mqtt("Armazenando topico: %s",(char *)topics_reconnect[t]);
  }
  /************************************ RECONEXÃO******************************/

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

  debug_mqtt("Endereco do broker IPv6: ");
  uip_debug_ipaddr_print(&broker_addr);
  debug_mqtt("Endereco da porta:%d ",g_mqtt_sn_con.udp_port);
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
  // Inicialmente precisamos enviar a requisição de CONNECT ao broker MQTT-SN pa
  // ra que seja possível qualquer outra operação.
  mqtt_sn_task_t connect_task;

  // debug_mqtt("Criando tarefa de CONNECT");
  connect_task.msg_type_q = MQTT_SN_TYPE_CONNECT;
  mqtt_sn_insert_queue(connect_task);
  /****************************************************************************/

  /****************************************************************************/
  // Implementação do recurso de [LWT]
  // Verificando se o usuário quer utilizar will topic e will message
  if (g_mqtt_sn_con.will_topic && g_mqtt_sn_con.will_message){
    mqtt_sn_task_t will_topic_task;
    will_topic_task.msg_type_q = MQTT_SN_TYPE_WILLTOPIC;
    mqtt_sn_insert_queue(will_topic_task);

    mqtt_sn_task_t will_message_task;
    will_message_task.msg_type_q = MQTT_SN_TYPE_WILLMSG;
    mqtt_sn_insert_queue(will_message_task);
  }

  /****************************************************************************/
  // Criando tarefas de [REGISTER]
  //
  // Para cada tópico definido pelo usuário no código principal.Inicia-se o pro
  // cesso de preenchimento de tarefas na fila de serviços MQT-SN.
  // Primeiro antes de qualquer processo MQTT-SN registra-se todos os tópicos in
  // formados pelo usuário, otimizando as funções de inscrição e publicação, o
  // broker irá então responder com os respectivos SHORT TOPIC para utilizarmos.
  mqtt_sn_task_t topic_reg;

  // debug_mqtt("Criando tarefa de REGISTER");
  size_t i;
  for(i = 0; i < topic_len; i++){
    if (g_mqtt_sn_con.will_topic && g_mqtt_sn_con.will_message)
      g_topic_bind[g_task_id-2].topic_name = topics_reconnect[i]; // Antecipa-se 2 no indíce em função das 2 tasks já alocadas para WILL do LWT
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

  // Alocação de número de evento disponível para os eventos do MQTT-SN
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
        debug_mqtt("Limite maximo de pacotes CONNECT");
      }
      else{
        debug_mqtt("Expirou tempo de CONNECT");
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
        debug_mqtt("Limite maximo de pacotes REGISTER");
      }
      else{
        debug_mqtt("Expirou tempo de REGISTER");
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
        debug_mqtt("Limite maximo de pacotes SUBSCRIBE");
      }
      else{
        debug_mqtt("Expirou tempo de SUBSCRIBE");
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
        debug_mqtt("Limite maximo de pacotes CONNECT para WILL TOPIC");
      }
      else{
        debug_mqtt("Expirou tempo de CONNECT para WILL TOPIC");
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
      debug_mqtt("Expirou tempo de estado desconhecido");
    break;
  }
}

void timeout_ping_mqtt(void *ptr){
  //debug_mqtt("\nTentativas PING:%d",g_tries_ping);
  if (g_ping_flag_resp) {
    g_ping_flag_resp = false;
    g_tries_ping = 0;
    //debug_mqtt("Enviando PING REQUEST");
    mqtt_sn_ping_send();
  }
  else{
    if (g_tries_ping >= MQTT_SN_RETRY_PING) {
      g_tries_ping = 0;
      ctimer_stop(&mqtt_time_ping);
      if (mqtt_status != MQTTSN_DISCONNECTED)
        process_post(&mqtt_sn_main,mqtt_event_ping_timeout,NULL);
      debug_mqtt("Limite tentativas de PING RESPONSE");
    }
    else{
      debug_mqtt("INCREMENTANDO PING");
      mqtt_sn_ping_send();
      g_tries_ping++;
    }
  }
  ctimer_reset(&mqtt_time_ping);
}

PROCESS_THREAD(mqtt_sn_main, ev, data){
  PROCESS_BEGIN();

  debug_mqtt("Inicio do processo MQTT-SN");

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
        debug_mqtt("Conectado ao broker MQTT-SN");
        ctimer_stop(&mqtt_time_connect);
        mqtt_sn_delete_queue(); // Deleta requisição de CONNECT já que estamos conectados;
        // Iniciamos o PING Request a partir deste momento
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
        debug_mqtt("Topico registrado no broker");

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
        debug_task("Task a executar:%s",teste);
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
            debug_task("Nenhuma tarefa a ser processada!");
          break;
        }
      }

      /********************** PUBLISH QoS 0 - MQTT-SN *************************/
      else if(ev == mqtt_event_pub_qos_0 &&
              mqtt_queue_first->data.msg_type_q == MQTT_SN_TYPE_PUBLISH){
        // Este evento de "mqtt_event_pub_qos_0" só ocorre quando não conhecemos
        // o tópico e precisamos registra, caso contrário a API desenvolvida
        // envia direto pro broker sem criar task, testes mostraram que a criação
        // e exclusão de tasks consumia recurso do mcu e afetava desempenho e igual
        // o nível de QoS é 0.
        debug_mqtt("Primeira publicacao de novo topico");
        // Pegamos o nome completo do tópico para a primeira publicação
        // do tópico recém registrado
        size_t j = 0;
        for (j=0; j < MAX_TOPIC_USED; j++)
          if (g_topic_bind[j].short_topic_id == 0xFF)
            break;
        mqtt_sn_pub_send(g_topic_bind[j-1].topic_name,
                         g_message_bind,
                         mqtt_queue_first->data.retain,
                         mqtt_queue_first->data.qos_level);
        mqtt_sn_delete_queue(); // Deleta requisição de PUBLISH
        if (!mqtt_sn_check_empty())
          process_post(&mqtt_sn_main, mqtt_event_run_task, NULL); // Inicia outras tasks caso a fila não esteja vazia
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
        mqtt_sn_delete_queue(); // Deleta requisição de SUBSCRIBE
        ctimer_stop(&mqtt_time_subscribe);
        debug_mqtt("Topico inscrito no broker");
        if (!mqtt_sn_check_empty())
          process_post(&mqtt_sn_main, mqtt_event_run_task, NULL); // Gera evento de processo de tasks
        else
          mqtt_status = MQTTSN_TOPIC_REGISTERED; // Libera publicações e outra operações, caso não haja mais tasks para fazer
      }

      /********************** PING REQUEST - MQTT-SN **************************/
      else if(ev == mqtt_event_ping_timeout){
        ctimer_stop(&mqtt_time_connect);
        ctimer_stop(&mqtt_time_register);
        ctimer_stop(&mqtt_time_ping);
        ctimer_stop(&mqtt_time_subscribe);

        mqtt_status = MQTTSN_DISCONNECTED;
        debug_mqtt("Desconectado broker");
        #ifdef MQTT_SN_AUTO_RECONNECT
          g_recon = true;
          init_vectors();
          mqtt_sn_create_sck(g_mqtt_sn_con, topics_reconnect, topics_len, callback_mqtt);
        #endif
      }
  }
  PROCESS_END();
}
