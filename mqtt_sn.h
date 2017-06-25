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
 * @file mqtt_sn.h
 * @brief Conjunto de protótipos e definiçoes do protocolo MQTT-SN
 * @author Ânderson Ignácio da Silva
 * @date 19 Ago 2016
 * @brief Arquivo principal do código fonte do porte do MQTT-SN para o Contiki
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
*  Macros de debug utilizadas para o MQTT-SN
*  @{
*/
/*!
  @brief Se definida habilita mensagens de debug da rede MQTT-SN
*/
//#define DEBUG_MQTT_SN
/*!
  @brief Se definida habilita mensagens de debug do sistema operacional
*/
#define DEBUG_OS
/*!
  @brief Se definida habilita mensagens de debug de tarefas da fila utilizada pelo MQTT-SN
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
*  Macros de protocolos utilizadas para o MQTT-SN
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
*  Macros de controle utilizadas para o MQTT-SN
*  @{
*/
#define ss(x) sizeof(x)/sizeof(*x)               /**< Computa o tamanho de um vetor de ponteiros */
#define MQTT_SN_AUTO_RECONNECT                   /**< Define se o dispositivo deve se auto conectar de tempos em tempos */
#define MQTT_SN_RETRY_PING        5              /**< Número de tentativas de envio de PING REQUEST antes de desconectar nó <-> broker */
#define MQTT_SN_TIMEOUT_CONNECT   9*CLOCK_SECOND /**< Tempo base para comunicação MQTT-SN broker <-> nó */
#define MQTT_SN_TIMEOUT           3*CLOCK_SECOND   /**< Tempo base para comunicação MQTT-SN broker <-> nó */
#define MQTT_SN_RETRY             5              /**< Número de tentativas de enviar qualquer pacote ao broker antes de desconectar */
#define MAX_QUEUE_MQTT_SN         100            /**< Número máximo de tarefas a serem inseridas alocadas dinamicamente MQTT-SN */
#define MAX_TOPIC_USED            100            /**< Número máximo de tópicos que o usuário pode registrar, a API cria um conjunto de estruturas para o bind de topic e short topic id */
/** @}*/

/*! \addtogroup Pacotes
*  Macros de debug utilizadas para o MQTT-SN
*  @{
*/
/** @struct disconnect_packet_t
 *  @brief Estrutura de pacote de desconexão do broker MQTT-SN
 *  @var disconnect_packet_t::length
 *    Comprimento do pacote
 *  @var disconnect_packet_t::msg_type
 *    Tipo de mensagem
 *  @var disconnect_packet_t::duration
 *    Duração do tempo de desconexão, utilizado para sleeping devices (ver especificação do broker)
 */
typedef struct __attribute__((packed)){
  uint8_t length;
  uint8_t  msg_type;
  uint16_t duration;
} disconnect_packet_t;

/** @struct ping_req_t
 *  @brief Estrutura de pacote de desconexão do broker MQTT-SN
 *  @var ping_req_t::length
 *    Comprimento do pacote
 *  @var ping_req_t::msg_type
 *    Tipo de mensagem
 *  @var ping_req_t::client_id
 *    Nome do identificador de cliente para conexão MQTT-SN
 */
typedef struct __attribute__((packed)){
  uint8_t length;
  uint8_t  msg_type;
  char client_id[23];
} ping_req_t;

/** @struct publish_packet_t
 *  @brief Estrutura de pacote de publicação MQTT-SN
 *  @var publish_packet_t::length
 *    Comprimento do pacote
 *  @var publish_packet_t::type
 *    Tipo de mensagem
 *  @var publish_packet_t::flags
 *    Flags utilizadas (retain,DUP,QoS...)
 *  @var publish_packet_t::topic_id
 *    Identificador do topic id registrado no broker, para publicar necessita-se o registro prévio
 *  @var publish_packet_t::message_id
 *    Identificador de Mensagem
 *  @var publish_packet_t::data
 *    Dado a ser publicado no tópico definido
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
 *  @brief Estrutura de pacote de inscrição do tipo Wildcard MQTT-SN
 *  @var subscribe_wildcard_packet_t::length
 *    Comprimento do pacote
 *  @var subscribe_wildcard_packet_t::type
 *    Tipo de mensagem
 *  @var subscribe_wildcard_packet_t::flags
 *    Flags utilizadas (retain,DUP,QoS...)
 *  @var subscribe_wildcard_packet_t::message_id
 *    Identificador de mensagem utilizada para receber o pacote correspondente
 *  @var subscribe_wildcard_packet_t::topic_name
 *    Tópico do tipo wildcard para se inscrever
 */
typedef struct __attribute__((packed)) {
  uint8_t length;
  uint8_t type;
  uint8_t flags;
  uint16_t message_id;
  char topic_name[MQTT_SN_MAX_TOPIC_LENGTH];
} subscribe_wildcard_packet_t;

/** @struct subscribe_packet_t
 *  @brief Estrutura de pacote MQTT-SN do tipo SUBSCRIBE
 *  @var subscribe_packet_t::length
 *    Comprimento do pacote
 *  @var subscribe_packet_t::type
 *    Tipo de mensagem
 *  @var subscribe_packet_t::flags
 *    Flags utilizadas (retain,DUP,QoS...)
 *  @var subscribe_packet_t::message_id
 *    Identificador de mensagem utilizada para receber o pacote correspondente
 *  @var subscribe_packet_t::topic_id
 *    Topic ID pré-registrado com o tópico correspondente a inscrição (tópico deve estar inserido na lista de registro)
 */
typedef struct __attribute__((packed)) {
  uint8_t length;
  uint8_t type;
  uint8_t flags;
  uint16_t message_id;
  uint16_t topic_id;
} subscribe_packet_t;

/** @struct connect_packet_t
 *  @brief Estrutura de pacotes MQTT-SN do tipo CONNECT
 *  @var connect_packet_t::length
 *    Comprimento total do pacote MQTT-SN
 *  @var connect_packet_t::type
 *    Descreve o tipo de mensagem que será enviado ao broker
 *  @var connect_packet_t::flags
 *    Contém os parâmetros de flag que serão enviados como (DUP,QoS,Retain,Will,
 *    CleanSession, TopicType)
 *  @var connect_packet_t::protocol_id
 *    Presente somente no CONNECT indicando versão do protocolo e o nome
 *  @var connect_packet_t::duration
 *    Indica a duração de um período em segundos podendo ser de até 18 Horas
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
 *  @brief Estrutura de pacotes MQTT-SN do tipo REGISTER
 *  @var register_packet_t::length
 *    Comprimento total do pacote MQTT-SN
 *  @var register_packet_t::type
 *    Descreve o tipo de mensagem que será enviado ao broker
 *  @var register_packet_t::topic_id
 *    Short Topic que será utilizado para envio do REGISTER - Quando enviado pelo nó, usa-se 0x0000
 *  @var register_packet_t::message_id
 *    Identificador único do REGACK correspondente enviado pelo broker normalmente
 *  @var register_packet_t::topic_name
 *    Nome do tópico a ser registrado
 */
typedef struct __attribute__((packed)){
  uint8_t length;
  uint8_t type;
  uint16_t topic_id;
  uint16_t message_id;
  char topic_name[MQTT_SN_MAX_TOPIC_LENGTH];
} register_packet_t;

/** @struct willtopic_packet_t
 *  @brief Estrutura de pacotes MQTT-SN do tipo WILL TOPIC
 *  @var willtopic_packet_t::length
 *    Comprimento total do pacote MQTT-SN
 *  @var willtopic_packet_t::type
 *    Descreve o tipo de mensagem que será enviado ao broker
 *  @var willtopic_packet_t::flags
 *    Contém os parâmetros de flag que serão enviados como (DUP,QoS,Retain,Will,
 *    CleanSession, TopicType)
 *  @var willtopic_packet_t::will_topic
 *    Tópico no qual será publicada a mensagem quando o dispositivo se desconectar
 */
typedef struct __attribute__((packed)){
  uint8_t length;
  uint8_t type;
  uint8_t flags;
  char will_topic[MQTT_SN_MAX_TOPIC_LENGTH];
} willtopic_packet_t;

/** @struct willmessage_packet_t
 *  @brief Estrutura de pacotes MQTT-SN do tipo WILL MESSAGE
 *  @var willmessage_packet_t::length
 *    Comprimento total do pacote MQTT-SN
 *  @var willmessage_packet_t::type
 *    Descreve o tipo de mensagem que será enviado ao broker
 *  @var willmessage_packet_t::will_message
 *    Contém a mensagem que será publicada quando o dispositivo se desconectar
 */
typedef struct __attribute__((packed)){
  uint8_t length;
  uint8_t type;
  char will_message[MQTT_SN_MAX_PACKET_LENGTH];
} willmessage_packet_t;

/** @struct regack_packet_t
 *  @brief Estrutura de pacotes MQTT-SN do tipo REGACK
 *  @var regack_packet_t::length
 *    Comprimento total do pacote MQTT-SN
 *  @var regack_packet_t::type
 *    Descreve o tipo de mensagem que será enviado ao broker
 *  @var regack_packet_t::topic_id
 *    Short Topic que será utilizado para recebimento de mensagem REGISTER - Quando enviado pelo broker envia ao nó
 *  @var regack_packet_t::message_id
 *    Identificador único do REGACK correspondente enviado pelo broker normalmente
 *  @var regack_packet_t::return_code
 *    Código de retorno da mensagem
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
 *  @brief Tipo de função de callback que deve ser repassada ao broker
 */
typedef void (*mqtt_sn_cb_f)(char *,char *);

/** @struct mqtt_sn_task_t
 *  @brief Estrutura de tarefa de fila MQTT-SN
 *  @var mqtt_sn_task_t::msg_type_q
 *    Tipo de mensagem a ser alocada
 *  @var mqtt_sn_task_t::short_topic
 *    Tópico curto ou topic id a ser publicado/inscrito
 *  @var mqtt_sn_task_t::qos_level
 *    Nível QoS da tarefa a ser alocada na pilha
 *  @var mqtt_sn_task_t::id_task
 *    Identificador da tarefa
 */
typedef struct {
  uint8_t  msg_type_q;
  uint8_t  short_topic;
  uint16_t id_task;
  uint8_t  qos_level;
  uint8_t  retain;
} mqtt_sn_task_t;

/** @struct node
 *  @brief Estrutura de fila MQTT-SN
 *  @var data
 *    Tarefa a ser processada
 *  @var link
 *    Link para a próxima tarefa
 */
struct node {
    mqtt_sn_task_t data;
    struct node *link;
}*mqtt_queue_first, *mqtt_queue_last;

/** @typedef resp_con_t
 *  @brief Tipo de erros de funções
 *  @var SUCCESS_CON::FAIL_CON
 *    Erro ao processar algo
 *  @var SUCCESS_CON::SUCCESS_CON
 *    Sucesso ao processar algo
 *  @todo Implementar mais tipos de erros
 */
typedef enum resp_con{
   FAIL_CON,
   SUCCESS_CON,
} resp_con_t;

/** @typedef short_topics_t
 *  @brief Estrutura para controle de tópicos e tópicos curtos
 */
typedef struct {
   char *topic_name;
   uint8_t short_topic_id;
   uint8_t subscribed;
} short_topics_t;

/** @typedef mqtt_sn_status_t
 *  @brief Estados da ASM do MQTT-SN
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
 *  @brief Estrutura de conexão ao broker MQTT-SN
 *  @var mqtt_sn_con_t::simple_udp_connection
 *    Handle da conexão UDP com o broker
 *  @var mqtt_sn_con_t::udp_port
 *    Porta UDP de conexão com o broker (default:1884)
 *  @var mqtt_sn_con_t::ipv6_broker
 *    Endereço IPv6 do broker UDP
 *  @var mqtt_sn_con_t::keep_alive
 *    Tempo de requisição Keep Alive para PINGREQ e PINGRESP
 *  @var mqtt_sn_con_t::client_id
 *    Identificador de cliente para conexão com o broker MQTT-SN
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

/** @brief Insere uma tarefa na fila
 *
 * 		Insere uma nova tarefa na fila de requisições a serem processadas.
 *
 *  @param [in] new Nova tarefa a ser processada pela ASM do MQTT-SN
 *
 *  @retval FAIL_CON         Não foi possível alocar uma nova tarefa a fila
 *  @retval SUCCESS_CON      Foi possível alocar uma nova tarefa a fila
 *
 * 	@todo		Melhorar alocação dinâmica de memória
 **/
resp_con_t mqtt_sn_insert_queue(mqtt_sn_task_t new);

/** @brief Remove o elemento mais próximo de ser processado
 *
 * 		Realiza a remoção do elemento mais próximo de ser processado, no caso o
 *    mais antigo inserido na fila
 *
 *  @param [in] 0 Não recebe argumento
 *
 *  @retval 0 Não retorna nada
 *
 * 	@todo	Adicionar opção de exclusão intermediária
 **/
void mqtt_sn_delete_queue();

/** @brief Lista as tarefas da fila
 *
 * 		Percorre os links dos ponteiros listando os elementos a serem
 *    processados pela ASM do MQTT-SN
 *
 *  @param [in] 0 Não recebe argumento
 *
 *  @retval 0 Não retorna nada
 *
 **/
void mqtt_sn_check_queue();

/** @brief Envia requisição de conexão ao broker MQTT-SN
 *
 * 		Realiza o envio de mensagens do tipo CONNECT ao broker MQTT-SN
 *
 *  @param [in] rc Código de retorno da requisição MQTT (Return Code)
 *
 *  @retval FAIL_CON      Falha por algum motivo no código de retorno
 *  @retval SUCCESS_CON   Sucesso no recebimento do código de retorno
 *
 * 	@todo		Expandir o tipo de falha para tornar mais precisa a depuração
 *          futura
 **/
resp_con_t mqtt_sn_check_rc(uint8_t rc);

/** @brief Realiza o parsing das mensagens UDP recebidas
 *
 * 		Realiza o parsing das mensagens UDP recebidas de acordo com
 *    o protocolo MQTT-SN, alterando o status da conexão geral com
 *    o broker.
 *
 *  @param [in] data Ponteiro para o conteúdo UDP recebido
 *
 *  @retval 0 Não retorna nada
 **/
void mqtt_sn_recv_parser(const uint8_t *data);

/** @brief Inicia conexão ao broker UDP
 *
 * 		Estabelece a conexão com um servidor MQTT-SN, através
 *    da porta 1884 além de iniciar a fila
 *    de processos de conexão do protocolo.
 *
 *  @param [in] mqtt_sn_connection Estrutura padrão de comunicação MQTT-SN
 *  @param [in] topics Vetor de tópicos a serem registrados
 *  @param [in] topic_len Tamanho do vetor de tópicos a serem registrados
 *  @param [in] mqtt_sn_cb_f Ponteiro para função de callback para recebimento das mensagens MQTT-SN
 *
 *  @retval FAIL_CON      Falha ao alocar conexão UDP
 *  @retval SUCCESS_CON   Sucesso ao alocar conexão UDP
 *
 **/
resp_con_t mqtt_sn_create_sck(mqtt_sn_con_t mqtt_sn_connection, char *topics[],size_t topic_len, mqtt_sn_cb_f cb_f);

/** @brief Envio de mensagens ao broker do tipo REGISTER
 *
 * 		Envia ao broker mensagens do tipo REGISTER com o topic name informado conforme a tarefa
 *    primeira na fila
 *
 *  @param [in] 0 Não recebe parâmetro
 *
 *  @retval FAIL_CON      Falha ao enviar o pacote REGISTER
 *  @retval SUCCESS_CON   Sucesso ao enviar o pacote REGISTER
 *
 **/
resp_con_t mqtt_sn_reg_send(void);

/** @brief Checa o status da conexão MQTT-SN
 *
 * 		Retorna o status da conexão MQTT-SN baseado na estrutura mqtt_sn_status_t
 *
 *  @param [in] 0 Não recebe argumento
 *
 *  @retval mqtt_sn_status_t Estado da conexão
 *
 **/
mqtt_sn_status_t mqtt_sn_check_status(void);

/** @brief Envia requisição de conexão ao broker MQTT-SN
 *
 * 		Realiza o envio de mensagens do tipo CONNECT ao broker MQTT-SN
 *
 *  @param [in] 0 Não recebe argumento
 *
 *  @retval FAIL_CON      Falha ao enviar o pacote CONNECT
 *  @retval SUCCESS_CON   Sucesso ao enviar o pacote CONNECT
 *
 **/
resp_con_t mqtt_sn_con_send(void);

/** @brief Checa o status da fila de tarefas MQTT-SN
 *
 * 		Percorra a lista encadeada de tarefas para verificar se está vazia
 *
 *  @param [in] 0 Não recebe argumento
 *
 *  @retval TRUE  Fila vazia
 *  @retval FALSE  Há tarefas a serem processadas
 *
 **/
bool mqtt_sn_check_empty(void);

/** @brief Retorna a string de status correspondente
 *
 * 		Realiza o parsing do estado da conexão MQTT-SN traduzindo de typedef enum para estado em string
 *
 *  @param [in] type Não recebe argumento
 *  @param [in] type_string Não recebe argumento
 *
 *  @retval 0 Não retorna nada
 *
 **/
void parse_mqtt_type_string(uint8_t type, char **type_string);

/** @brief Inicializa PROCESS_THREAD MQTT-SN
 *
 * 		Inicializa a PROCESS_THREAD de MQTT-SN bem como as variáveis utilizadas e a alocaçãod e eventos
 *
 *  @param [in] 0 Não recebe argumento
 *
 *  @retval 0 Não retorna nada
 *
 **/
void mqtt_sn_init(void);

/** @brief Envia pacote PUBLISH ao broker MQTT-SN
 *
 * 		Monta o pacote e envia ao broker a mensagem de publicação
 *
 *  @param [in] topic Tópico a ser publicado
 *  @param [in] message Mensagem a ser publicada
 *  @param [in] qos Nível de QoS da publicação
 *
 *  @retval FAIL_CON      Falha ao enviar a publicação
 *  @retval SUCCESS_CON   Sucesso ao enviar a publicação
 *
 **/
resp_con_t mqtt_sn_pub_send(char *topic,char *message, bool retain_flag, uint8_t qos);

/** @brief Checa o status da conexãoe em String
 *
 * 		Verifica o status da conexão MQTT-SN e retorna uma string com o estado
 *
 *  @param [in] Não recebe argumento
 *
 *  @retval STRING  String do estado atual da conexão MQTT-SN
 *
 **/
char* mqtt_sn_check_status_string(void);

/** @brief Gera a flag de nível QoS
 *
 * 		Retorna o estado da flag correspondente ao nível QoS enviado
 *
 *  @param [in] qos Nível QoS desejado
 *
 *  @retval QoS Retorna a flag do nível QoS desejado
 *
 **/
uint8_t mqtt_sn_get_qos_flag(int8_t qos);

/** @brief Prepara requisição de publicação ao broker MQTT-SN
 *
 * 		Formata e gera a tarefa na fila para publicação no tópico pré-registrado
 *
 *  @param [in] topic Tópico a ser publicado
 *  @param [in] message Mensagem a ser publicada
 *  @param [in] retain_flag Identificador de mensagem retentiva
 *  @param [in] qos Nível de QoS da publicação
 *
 *  @retval FAIL_CON      Falha ao gerar a tarefa de publicação
 *  @retval SUCCESS_CON   Sucesso ao gerar a tarefa de publicação
 *
 **/
resp_con_t mqtt_sn_pub(char *topic,char *message, bool retain_flag, uint8_t qos);

/** @brief Exibe os tópicos registrados
 *
 * 		Exibe a lista de tópicos registrados no broker
 *
 *  @param [in] 0 Não recebe argumento
 *
 *  @retval 0 Não retorna nada
 *
 **/
void print_g_topics(void);

/** @brief Processa timeout de pacotes
 *
 * 		Processa toda e qualquer requisição de expiração de tempo por timeout de envio de mensagens (SUBSCRIBE, PUBLISH, REGISTER)
 *
 *  @param [in] 0 Não recebe argumento
 *
 *  @retval 0 Não retorna nada
 *
 **/
void timeout_con(void *ptr);

/** @brief Processa timeout de ping
 *
 * 		Processa toda expiração de tempo por timeout de envio de mensagens PINGREQ
 *
 *  @param [in] 0 Não recebe argumento
 *
 *  @retval 0 Não retorna nada
 *
 **/
void timeout_ping_mqtt(void *ptr);

/** @brief Envia requisição de ping ao broker
 *
 * 		Envia requisição de ping ao broker diretamente por mensagens PING REQ
 *
 *  @param [in] 0 Não recebe argumento
 *
 *  @retval 0 Não retorna nada
 *
 **/
void mqtt_sn_ping_send(void);

/** @brief Libera opção de geração de tarefas
 *
 * 		Habilita a geração de tarefas na fila conforma o estado da conexão MQTT-SN
 *
 *  @param [in] 0 Não recebe argumento
 *
 *  @retval TRUE Pode-se gerar tarefa na fila
 *  @retval FALSE Estado da conexão MQTT-SN impossibilita geração de tarefas na fila
 *
 **/
bool unlock_tasks(void);

/** @brief Prepara requisição de inscrição ao broker MQTT-SN
 *
 * 		Formata e gera a tarefa na fila para inscrição no tópico pré-registrado
 *
 *  @param [in] topic Tópico a ser inscrito
 *  @param [in] qos Nível de QoS da inscrição
 *
 *  @retval FAIL_CON      Falha ao gerar a tarefa de inscrição
 *  @retval SUCCESS_CON   Sucesso ao gerar a tarefa de inscrição
 *
 **/
resp_con_t mqtt_sn_sub(char *topic, uint8_t qos);

/** @brief Envia pacote SUBSCRIBE ao broker MQTT-SN
 *
 * 		Monta o pacote e envia ao broker a mensagem de inscrição
 *
 *  @param [in] topic Tópico a ser inscrito (deve estar pré-listado e passado como argumento em mqtt_sn_create_sck)
 *  @param [in] qos Nível de QoS da publicação
 *
 *  @retval FAIL_CON      Falha ao enviar a inscrição
 *  @retval SUCCESS_CON   Sucesso ao enviar a inscrição
 *
 **/
resp_con_t mqtt_sn_sub_send(char *topic, uint8_t qos);

/** @brief Envia pacote SUBSCRIBE do tipo WILDCARD ao broker MQTT-SN
 *
 * 		Monta o pacote e envia ao broker a mensagem de inscrição do tipo Wildcard (#,+)
 *
 *  @param [in] topic Tópico a ser inscrito (deve estar pré-listado e passado como argumento em mqtt_sn_create_sck)
 *  @param [in] qos Nível de QoS da publicação
 *
 *  @retval FAIL_CON      Falha ao enviar a inscrição
 *  @retval SUCCESS_CON   Sucesso ao enviar a inscrição
 *
 **/
resp_con_t mqtt_sn_sub_send_wildcard(char *topic, uint8_t qos);

/** @brief Verifica se o tópico já foi registrado
 *
 * 		Verifica se o tópico em análise já foi previamente registrado ou está em processo de registro
 *
 *  @param [in] topic Tópico a ser inscrito (deve estar pré-listado e passado como argumento em mqtt_sn_create_sck)
 *
 *  @retval FAIL_CON      Tópico já foi registrado ou está em andamento
 *  @retval SUCCESS_CON   Tópico liberado para inscrição
 *
 **/
resp_con_t verf_hist_sub(char *topic);

/** @brief Envia pacote DISCONNECT ao broker MQTT-SN
 *
 * 		Monta o pacote e envia ao broker a mensagem de desconexão
 *
 *  @param [in] duration Tempo que irá ficar desconectado, utilizado para sleeping devices
 *
 *  @retval FAIL_CON      Falha ao enviar a desconexão
 *  @retval SUCCESS_CON   Sucesso ao enviar a desconexão
 *
 **/
resp_con_t mqtt_sn_disconnect_send(uint16_t duration);

/** @brief Inicializa os vetores MQTT-SN
 *
 * 		Deleta tarefas na fila e inicializa o vetor de tópicos setando 0xFF aos identificadores de tópico
 *
 *  @param [in] 0 Não recebe argumento
 *
 *  @retval 0 Não retorna argumento
 *
 **/
void init_vectors(void);

/** @brief Inicia o evento de SUBSCRIBE
 *
 * 		Inicia as requisições de inscrição através do evento
 *
 *  @param [in] 0 Não recebe argumento
 *
 *  @retval 0 Não retorna argumento
 *
 **/
void init_sub(void *ptr);

/** @brief Verifica pré-registro do tópico
 *
 * 		Verifica se o tópico já foi pré-registrado antes de iniciar qualquer operação
 *
 *  @param [in] topic Tópico a ser verificado
 *
 *  @retval FAIL_CON      Tópico não foi pré-registrado
 *  @retval SUCCESS_CON   Tópico registrado no vetor de tópicos
 *
 **/
resp_con_t verf_register(char *topic);

/** @brief Envia mensagem de LWT
 *
 * 		Envia mensagem a ser publicada quando o tópico se desconectar
 *
 *  @param [in] 0 Não recebe argumento
 *
 *  @retval FAIL_CON      Falha ao enviar a mensagem Will MSG
 *  @retval SUCCESS_CON   Sucesso ao enviar a mensagem Will MSG
 *
 **/
resp_con_t mqtt_sn_will_message_send(void);

/** @brief Envia tópico de LWT
 *
 * 		Envia tópico utilizado quando o dispositivo se desconectar
 *
 *  @param [in] 0 Não recebe argumento
 *
 *  @retval FAIL_CON      Falha ao enviar a mensagem Will MSG
 *  @retval SUCCESS_CON   Sucesso ao enviar a mensagem Will MSG
 *
 **/
resp_con_t mqtt_sn_will_topic_send(void);

/** @brief Callback de recepção UDP
 *
 * 		Recebe dados da conexão UDP com o broker
 *
 *  @param [in] simple_udp_connection Handle da conexão UDP
 *  @param [in] sender_addr Endereço IP do broker
 *  @param [in] sender_port Porta de envio
 *  @param [in] receiver_addr Endereço IP de recepção
 *  @param [in] receiver_port Porta de recepção
 *  @param [in] data Dado recebido
 *  @param [in] datalen Tamanho do dado recebido
 *
 *  @retval 0 Não retorna parâmetro
 *
 **/
void mqtt_sn_udp_rec_cb(struct simple_udp_connection *c,
                            const uip_ipaddr_t *sender_addr,
                            uint16_t sender_port,
                            const uip_ipaddr_t *receiver_addr,
                            uint16_t receiver_port,
                            const uint8_t *data,
                            uint16_t datalen);

/** @brief Prepara tarefa de SUBSCRIBE do tipo WILDCARD
 *
 * 		Gera tarefa de inscrição com Wildcard ao broker MQTT-SN
 *
 *  @param [in] topic Tópico a ser inscrito (deve estar pré-listado e passado como argumento em mqtt_sn_create_sck)
 *  @param [in] qos Nível de QoS da publicação
 *
 *  @retval FAIL_CON      Falha ao enviar a inscrição
 *  @retval SUCCESS_CON   Sucesso ao enviar a inscrição
 *
 **/
resp_con_t mqtt_sn_sub_wildcard(char *topic, uint8_t qos);

/** @brief Envia pacote do tipo REGACK ao broker
 *
 * 		Envia ao broker MQTT-SN a mensagem do tipo REGACK quando receber algum tópico via Wildcad
 *
 *  @param [in] msg_id Message id correspondente do envio
 *  @param [in] topic_id Topic ID enviado pelo broker para registrar no vetor de tópicos o tópico novo
 *
 *  @retval FAIL_CON      Falha ao enviar a regack
 *  @retval SUCCESS_CON   Sucesso ao enviar a regack
 *
 **/
resp_con_t mqtt_sn_regack_send(uint16_t msg_id, uint16_t topic_id);

#endif
