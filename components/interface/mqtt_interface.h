#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef void mqtt_inface_t;
typedef void mqtt_client_t;

typedef enum
{
    MQTT_EVT_ERROR = 0,      /*!< on error event, additional context: connection return code, error
                                handle from esp_tls (if supported) */
    MQTT_EVT_CONNECTED,      /*!< connected event, additional context:
                                session_present flag */
    MQTT_EVT_DISCONNECTED,   /*!< disconnected event */
    MQTT_EVT_SUBSCRIBED,     /*!< subscribed event, additional context:
                                  - msg_id               message id
                                  - data                 pointer to the received
                                data
                                  - data_len             length of the data for this
                                event
                                  */
    MQTT_EVT_UNSUBSCRIBED,   /*!< unsubscribed event */
    MQTT_EVT_PUBLISHED,      /*!< published event, additional context:  msg_id */
    MQTT_EVT_DATA,           /*!< data event, additional context:
                                  - msg_id               message id
                                  - topic                pointer to the received topic
                                  - topic_len            length of the topic
                                  - data                 pointer to the received data
                                  - data_len             length of the data for this event
                                  - current_data_offset  offset of the current data for
                                this event
                                  - total_data_len       total length of the data received
                                  - retain               retain flag of the message
                                  - qos                  QoS level of the message
                                  - dup                  dup flag of the message
                                  Note: Multiple MQTT_EVT_DATA could be fired for one
                                message, if it is         longer than internal buffer. In that
                                case only first event contains topic         pointer and length,
                                other contain data only with current data length         and
                                current data offset updating.
                                   */
    MQTT_EVT_BEFORE_CONNECT, /*!< The event occurs before connecting */
    MQTT_EVT_DELETED,        /*!< Notification on delete of one message from the
                                internal outbox,        if the message couldn't have been sent
                                and acknowledged before expiring        defined in
                                OUTBOX_EXPIRED_TIMEOUT_MS.        (events are not posted upon
                                deletion of successfully acknowledged messages)
                                  - This event id is posted only if
                                MQTT_REPORT_DELETED_MESSAGES==1
                                  - Additional context: msg_id (id of the deleted
                                message).
                                  */
   MQTT_EVT_NONE
} mqtt_event_def;

typedef enum
{
    MQTT_ERR_NONE = 0,
    MQTT_ERR_TCP_TRANSPORT,
    MQTT_ERR_CONNECTION_REFUSED,
} mqtt_err_def;

typedef struct
{
    char *topic;   /*!< Topic associated with this event */
    int topic_len; /*!< Length of the topic for this event associated with this
                    event */
    char *data;    /*!< Data associated with this event */
    int data_len;  /*!< Length of the data for this event */
    int qos;       /*!< QoS of the messages associated with this event */
} mqtt_msg_t;

typedef struct
{
    void *param;
    void (*func)(void *param, mqtt_event_def event, mqtt_msg_t *msg);
} mqtt_event_cb_t;

typedef struct
{
    const char *uri;                        /*!< Complete *MQTT* broker URI */
    const char *host;                       /*!< Hostname, to set ipv4 pass it as string) */
    uint16_t port;                          /*!< *MQTT* server port */
    const char *client_id;                  /*!< Set *MQTT* client identifier. Ignored if set_null_client_id == true If NULL set
                                             the default client id. Default client id is ``ESP32_%CHIPID%`` where `%CHIPID%` are
                                             last 3 bytes of MAC address in hex format */
    const char *username;                   /*!< *MQTT* username */
    const char *password;                   /*!< *MQTT* password */
    bool disable_clean_session;             /*!< *MQTT* clean session, default clean_session is true */
    int keepalive;                          /*!< *MQTT* keepalive, default is 120 seconds */
    bool disable_keepalive;                 /*!< Set `disable_keepalive=true` to turn off keep-alive mechanism, keepalive is active
                                             by default. Note: setting the config value `keepalive` to `0` doesn't disable
                                             keepalive feature, but uses a default keepalive period */
    const char *verification_certificate;   /*!< Certificate data, default is NULL, not required to verify the server. */
    long unsigned int verification_certificate_len;    /*!< Length of the buffer pointed to by certificate. */
    const char *authentication_certificate; /*!< Certificate for ssl mutual authentication, not required if mutual
                                             authentication is not needed. Must be provided with `key`.*/
    long unsigned int authentication_certificate_len;  /*!< Length of the buffer pointed to by certificate.*/
    const char *authentication_key;         /*!< Private key for SSL mutual authentication, not required if mutual authentication
                                             is not needed. If it is not NULL, also `certificate` has to be provided.*/
    long unsigned int authentication_key_len;          /*!< Length of the buffer pointed to by key.*/

    struct mqtt_will_t
    {
        const char *topic; /*!< LWT (Last Will and Testament) message topic */
        const char *msg;   /*!< LWT message, may be NULL terminated*/
        int msg_len;       /*!< LWT message length, if msg isn't NULL terminated must have the correct length */
        int qos;           /*!< LWT message QoS */
        int retain;        /*!< LWT retained message flag */
    } last_will;           /*!< Last will configuration */
} mqtt_cfg_t;

typedef struct
{
  void *user_data;
    void (*register_callback)(mqtt_event_cb_t *cb);
    bool (*init)(mqtt_cfg_t *cfg);
    bool (*open)(void);
    bool (*subscribe)(const char *topic, int qos);
    bool (*unsubscribe)(const char *topic);
    bool (*publish)(const char *topic, const char *data, int len, int qos, int retain);
    bool (*close)(void);
    mqtt_err_def (*error_code)(void);
    void (*delete)(void);
} mqtt_drv_t;

void mqtt_register_callback(mqtt_inface_t *obj, mqtt_event_cb_t *cb);
bool mqtt_init(mqtt_inface_t *obj, mqtt_cfg_t *cfg);
bool mqtt_open(mqtt_inface_t *obj);
bool mqtt_subscribe(mqtt_inface_t *obj, const char *topic, int qos);
bool mqtt_unsubscribe(mqtt_inface_t *obj, const char *topic);
bool mqtt_publish(mqtt_inface_t *obj, const char *topic, const char *data, int len, int qos, int retain);
int mqtt_read(mqtt_inface_t *obj, void *data, int length);
bool mqtt_close(mqtt_inface_t *obj);
mqtt_err_def mqtt_error_code(mqtt_inface_t *obj);
void mqtt_delete(mqtt_inface_t *obj);