/*
 *	mqtt_utk.c
 *	MQTT client wiring for micro T-Kernel (RP2040, NO_SYS=1)
 *
 *	Runs entirely on the single processor-1-owned task that already owns
 *	the CYW43439 and the rest of lwIP (docs/RELEASE.md safety rule 6):
 *	cyw43_utk_task() (cyw43_utk.c) calls mqtt_utk_start() once, the first
 *	time it observes CYW43_LINK_UP. mqtt.c's own keepalive/ping/retry
 *	timers, and the heartbeat below, both ride the same sys_check_timeouts()
 *	polling that task already does for DHCP and TCP -- nothing extra runs
 *	on any other task or core.
 *
 *	This file does not include tk/tkernel.h or any pico-sdk/cyw43 header
 *	(unlike cyw43_utk.c and sys_arch.c), so it isn't subject to either
 *	file's size_t workaround -- tm/tmonitor.h alone is namespace-safe here.
 */

#include "lwip/apps/mqtt.h"
#include "lwip/ip_addr.h"
#include "lwip/timeouts.h"
#include "wifi_secrets.h"
#include "mqtt_utk.h"
#include <tm/tmonitor.h>

#define MQTT_UTK_HEARTBEAT_MS   5000
#define MQTT_UTK_KEEPALIVE_S    60

/* IS19 extension (A11): every MQTT_UTK_PUMP_MS the pump below asks the
 * application for messages to publish (mqtt_utk_hook_next_publish) and
 * for a broker change (mqtt_utk_hook_broker).  At most MQTT_UTK_PUMP_BURST
 * messages per call; a message lwIP has no room for yet (ERR_MEM) is kept
 * and retried on the next call.  Both values: docs/decisions.md D-50. */
#define MQTT_UTK_PUMP_MS        10
#define MQTT_UTK_PUMP_BURST     4
#define MQTT_UTK_TOPIC_MAX      64
#define MQTT_UTK_PAYLOAD_MAX    MQTT_OUTPUT_RINGBUF_SIZE    /* never larger */
#define MQTT_UTK_IP_MAX         16

/* IS19 extension (report 6.1, BACKING OFF): after a failed or lost
 * connection wait 1, 2, 4 ... up to 30 s before the next attempt, and
 * start again from 1 s once connected.  This replaces the template's
 * fixed 3 s wait (MQTT_UTK_RETRY_MS).  docs/decisions.md D-70. */
#define MQTT_UTK_BACKOFF_FIRST_MS   1000
#define MQTT_UTK_BACKOFF_MAX_MS     30000

/* Inbound message staging. A publish arrives as one topic callback followed by
 * one or more data callbacks, so the topic has to be kept until the fragment
 * carrying MQTT_DATA_FLAG_LAST shows up. Both buffers are sized well past the
 * heartbeat this demo publishes; anything longer is truncated and flagged
 * rather than being allowed to run off the end. */
#define MQTT_UTK_RX_TOPIC_MAX   64
#define MQTT_UTK_RX_DATA_MAX    128

static mqtt_client_t	*client;
static ip_addr_t	broker_addr;
static u8_t		connecting;

static char		rx_topic[MQTT_UTK_RX_TOPIC_MAX];
static UB		rx_data[MQTT_UTK_RX_DATA_MAX];
static u16_t		rx_len;
static u8_t		rx_truncated;

/* IS19 extension (A11): broker address that can change at run time
 * (NFR14), and the message waiting to be published. */
static char		broker_ip[MQTT_UTK_IP_MAX] = MQTT_BROKER_IP;
static u16_t		broker_port = MQTT_BROKER_PORT;
static u32_t		backoff_ms = MQTT_UTK_BACKOFF_FIRST_MS;
static u8_t		pump_started;
static char		pump_topic[MQTT_UTK_TOPIC_MAX];
static UB		pump_payload[MQTT_UTK_PAYLOAD_MAX];
static int		pump_len = -1;

static void heartbeat_timeout(void *arg);
static void pump_timeout(void *arg);
static void back_off(void);

/* ---- Weak defaults: the template's behaviour (IS19 extension, A11) ---- */

__attribute__((weak)) int mqtt_utk_hook_next_publish(char *topic,
	int topic_size, unsigned char *payload, int payload_size)
{
	(void)topic; (void)topic_size; (void)payload; (void)payload_size;
	return -1;
}

__attribute__((weak)) void mqtt_utk_hook_receive(const char *topic,
	const unsigned char *payload, int len)
{
	(void)len;
	tm_printf((UB*)"mqtt: RX [%s] \"%s\"%s\n", topic, payload,
		  rx_truncated? " (truncated)": "");
}

__attribute__((weak)) const char *mqtt_utk_hook_subscribe_topic(void)
{
	return MQTT_TOPIC;
}

__attribute__((weak)) int mqtt_utk_hook_broker(char *ip, int ip_size,
	int *port)
{
	(void)ip; (void)ip_size; (void)port;
	return 0;
}

__attribute__((weak)) int mqtt_utk_hook_heartbeat(void)
{
	return 1;
}

__attribute__((weak)) void mqtt_utk_hook_link(int connected)
{
	(void)connected;
}

/* mqtt.c NUL-terminates the topic before handing it over, but it is only
 * valid for the duration of this call -- copy it out. */
static void incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
	u32_t	i;

	(void)arg;
	(void)tot_len;

	for(i = 0; i < (MQTT_UTK_RX_TOPIC_MAX - 1) && topic[i] != '\0'; i++) {
		rx_topic[i] = topic[i];
	}
	rx_topic[i] = '\0';

	rx_len = 0;
	rx_truncated = 0;
}

static void incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
	u16_t	i;

	(void)arg;

	for(i = 0; i < len; i++) {
		if(rx_len < (MQTT_UTK_RX_DATA_MAX - 1)) {
			/* The console is a terminal -- keep raw bytes from a
			 * payload this demo did not publish from scrambling it. */
			UB c = data[i];
			rx_data[rx_len++] = (c >= 0x20 && c < 0x7f)? c: '.';
		} else {
			rx_truncated = 1;
		}
	}

	if((flags & MQTT_DATA_FLAG_LAST) != 0) {
		rx_data[rx_len] = '\0';
		mqtt_utk_hook_receive(rx_topic, rx_data, (int)rx_len);
	}
}

static void subscribe_done_cb(void *arg, err_t result)
{
	(void)arg;
	if(result == ERR_OK) {
		tm_printf((UB*)"mqtt: subscribed to %s\n",
			  (UB*)mqtt_utk_hook_subscribe_topic());
	} else {
		tm_printf((UB*)"mqtt: subscribe failed, err=%d\n", (int)result);
	}
}

static void publish_done_cb(void *arg, err_t result)
{
	(void)arg;
	if(result != ERR_OK) {
		tm_printf((UB*)"mqtt: publish failed, err=%d\n", (int)result);
	}
}

static void heartbeat_timeout(void *arg)
{
	static unsigned int	count = 0;
	UB			payload[48];
	int			len;

	(void)arg;

	if(mqtt_client_is_connected(client)) {
		len = tm_sprintf(payload, (const UB*)"hb %u", count++);
		tm_printf((UB*)"mqtt: TX [" MQTT_TOPIC "] \"%s\"\n", payload);
		(void)mqtt_publish(client, MQTT_TOPIC, payload, (u16_t)len,
				   0 /* qos */, 0 /* retain */,
				   publish_done_cb, NULL);
	}

	/* Re-arms itself every call -- mqtt.c's connection callback (below)
	 * only starts this chain once, on the transition into MQTT_CONNECT_
	 * ACCEPTED, so there is exactly one heartbeat chain alive at a time. */
	sys_timeout(MQTT_UTK_HEARTBEAT_MS, heartbeat_timeout, NULL);
}

static void retry_timeout(void *arg)
{
	(void)arg;
	connecting = 0;
	mqtt_utk_start();
}

static void connection_cb(mqtt_client_t *c, void *arg, mqtt_connection_status_t status)
{
	(void)c;
	(void)arg;

	if(status == MQTT_CONNECT_ACCEPTED) {
		err_t	err;

		/* No longer *connecting* -- clear the guard so that if this
		 * connection later drops, the poll loop's mqtt_utk_start() call
		 * is allowed to reconnect. Leaving it set would wedge the client
		 * permanently after the first disconnect. */
		connecting = 0;
		backoff_ms = MQTT_UTK_BACKOFF_FIRST_MS;

		tm_printf((UB*)"mqtt: connected to %s\n", (UB*)broker_ip);
		mqtt_utk_hook_link(1);

		/* Subscribe to the same topic this client publishes to, so every
		 * heartbeat comes back from the broker and is printed by
		 * incoming_data_cb(). A matching RX for each TX is end-to-end
		 * proof of the round trip without needing mosquitto_sub on the
		 * broker host. Callbacks must be registered before subscribing,
		 * or the first delivery has nowhere to go. */
		mqtt_set_inpub_callback(client, incoming_publish_cb,
					incoming_data_cb, NULL);

		err = mqtt_subscribe(client, mqtt_utk_hook_subscribe_topic(),
				     0 /* qos */, subscribe_done_cb, NULL);
		if(err != ERR_OK) {
			tm_printf((UB*)"mqtt: mqtt_subscribe() failed, err=%d\n",
				  (int)err);
		}

		/* Drop any chain left over from a previous connection before
		 * arming a new one. Each reconnect would otherwise add another
		 * self-perpetuating heartbeat, and every one of them holds a
		 * MEMP_SYS_TIMEOUT slot until the pool is exhausted. */
		sys_untimeout(heartbeat_timeout, NULL);
		if(mqtt_utk_hook_heartbeat()) {
			sys_timeout(MQTT_UTK_HEARTBEAT_MS, heartbeat_timeout, NULL);
		}
	} else {
		mqtt_utk_hook_link(0);
		/* mqtt_client_connect() reconnects on its own for a dropped
		 * keepalive; a rejected/never-established connection does not
		 * retry itself, so do that here -- on a delay, so a broker
		 * that keeps rejecting (e.g. bad client ID) can't turn this
		 * into a tight reconnect loop. */
		tm_printf((UB*)"mqtt: connection status %d\n", (int)status);
		back_off();
	}
}

/* IS19 extension (report 6.1): BACKING OFF until retry_timeout().
 * connecting stays set meanwhile, so the poll loop's mqtt_utk_start()
 * calls do nothing; exactly one retry is pending at a time. */
static void back_off(void)
{
	connecting = 1;
	tm_printf((UB*)"mqtt: retrying in %d ms\n", (int)backoff_ms);
	sys_untimeout(retry_timeout, NULL);
	sys_timeout(backoff_ms, retry_timeout, NULL);
	backoff_ms = (backoff_ms >= (MQTT_UTK_BACKOFF_MAX_MS / 2))?
		     MQTT_UTK_BACKOFF_MAX_MS: (backoff_ms * 2);
}

void mqtt_utk_start(void)
{
	struct mqtt_connect_client_info_t	client_info = {0};
	err_t					err;

	if(connecting || (client != NULL && mqtt_client_is_connected(client))) {
		return;
	}

	if(client == NULL) {
		client = mqtt_client_new();
		if(client == NULL) {
			tm_printf((UB*)"mqtt: mqtt_client_new() failed\n");
			return;
		}
	}

	if(!pump_started) {
		pump_started = 1;
		sys_timeout(MQTT_UTK_PUMP_MS, pump_timeout, NULL);
	}

	if(!ip4addr_aton(broker_ip, ip_2_ip4(&broker_addr))) {
		tm_printf((UB*)"mqtt: bad broker address %s\n", (UB*)broker_ip);
		return;
	}
	IP_SET_TYPE(&broker_addr, IPADDR_TYPE_V4);

	client_info.client_id = MQTT_CLIENT_ID;
	client_info.keep_alive = MQTT_UTK_KEEPALIVE_S;

	connecting = 1;
	err = mqtt_client_connect(client, &broker_addr, broker_port,
				  connection_cb, NULL, &client_info);
	if(err != ERR_OK) {
		tm_printf((UB*)"mqtt: mqtt_client_connect() failed, err=%d\n", (int)err);
		back_off();
	}
}

/*
 * IS19 extension (A11): runs every MQTT_UTK_PUMP_MS on cyw43_utk_task,
 * like the heartbeat above.  Applies a broker change (NFR14), then
 * publishes the application's queued messages.  While the session is
 * down, queued messages are taken and dropped so they cannot pile up,
 * and the car keeps running (NFR10).
 */
static void pump_timeout(void *arg)
{
	char	new_ip[MQTT_UTK_IP_MAX];
	int	new_port = 0;
	int	sent = 0;

	(void)arg;

	if(mqtt_utk_hook_broker(new_ip, (int)sizeof(new_ip), &new_port)) {
		int	i;

		for(i = 0; (i < (MQTT_UTK_IP_MAX - 1)) && (new_ip[i] != '\0'); i++) {
			broker_ip[i] = new_ip[i];
		}
		broker_ip[i] = '\0';
		broker_port = (u16_t)new_port;
		tm_printf((UB*)"mqtt: broker changed to %s:%d\n",
			  (UB*)broker_ip, new_port);
		if(client != NULL) {
			if(mqtt_client_is_connected(client)) {
				mqtt_utk_hook_link(0);
			}
			/* Also stops a connection still in progress to the
			 * old broker; mqtt_disconnect() calls no callback. */
			mqtt_disconnect(client);
		}
		/* Connect to the new broker on the next poll, not after a
		 * back-off left over from the old one. */
		sys_untimeout(retry_timeout, NULL);
		backoff_ms = MQTT_UTK_BACKOFF_FIRST_MS;
		connecting = 0;
	}

	while(sent < MQTT_UTK_PUMP_BURST) {
		if(pump_len < 0) {
			pump_len = mqtt_utk_hook_next_publish(pump_topic,
				(int)sizeof(pump_topic), pump_payload,
				(int)sizeof(pump_payload));
		}
		if(pump_len < 0) {
			break;
		}
		if(client != NULL && mqtt_client_is_connected(client)) {
			err_t err = mqtt_publish(client, pump_topic, pump_payload,
						 (u16_t)pump_len, 0 /* qos */,
						 0 /* retain */, publish_done_cb,
						 NULL);
			if(err == ERR_MEM) {
				break;		/* no room yet: retry next time */
			}
		}
		pump_len = -1;
		sent++;
	}

	sys_timeout(MQTT_UTK_PUMP_MS, pump_timeout, NULL);
}
