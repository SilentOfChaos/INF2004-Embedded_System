If don't want MQTT, in app_config.h, find 
#define CFG_TLM_ECHO_CONSOLE            0
Change it from 0 to 1 :)

# MQTT from scratch: IS19 PicoCar (Pico W + micro T-Kernel)

This guide takes you from nothing to the car publishing telemetry and taking commands over MQTT. It matches the code in this repo: `build_make/pico_rp2040.mk`, `config/wifi_secrets.h`, `lib/liblwip/sysdepend/pico_rp2040/mqtt_utk.c` and `app_program/subsystem1/mqtt_bridge.c`.

```
 Laptop/phone (MQTT client) <--Wi-Fi--> Broker (Mosquitto, port 1883) <--Wi-Fi--> Pico W car
   subscribes inf2004/IS19/#                                                    publishes telemetry/status/event/ack
   publishes  inf2004/IS19/cmd                                                  subscribes inf2004/IS19/cmd
```

---

## 0. What you need

- A Pico W car with this firmware repo
- A computer with **WSL (Ubuntu)** on Windows, Linux or macOS. Run all build commands in WSL on Windows.
- A **2.4 GHz** Wi-Fi network. The Pico W can't see 5 GHz. A phone hotspot works, but on an iPhone turn on *Maximise Compatibility*.
- A laptop **on the same network** to run the broker and an MQTT client

---

## 1. One-time toolchain set-up (skip if can already run upload.)

```sh
sudo apt update && sudo apt install -y git make g++ gcc-arm-none-eabi

mkdir -p ~/pico && cd ~/pico
git clone --branch 2.2.0 https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk
# tinyusb + cyw43 are needed for every build; lwip is the extra one MQTT needs
git submodule update --init lib/tinyusb lib/cyw43-driver lib/lwip
```

Add this to the end of `~/.bashrc`, then open a new terminal:

```sh
export PICO_SDK_PATH="$HOME/pico/pico-sdk"
```

Check:

```sh
arm-none-eabi-gcc --version
ls $PICO_SDK_PATH/lib/lwip/src/core/init.c    # must exist, or the MQTT build stops with an error
```

---

## 2. Set up the MQTT broker (Mosquitto on your laptop)
Die die need mosquitto, MQTT explorer is utu

### 2.1 Install

- **Windows:** download the installer from <https://mosquitto.org/download/> (installs to `C:\Program Files\mosquitto`).
- **WSL / Linux:** `sudo apt install -y mosquitto mosquitto-clients`
- **macOS:** `brew install mosquitto`

### 2.2 Let other devices connect

Mosquitto 2.x only accepts connections from the same machine by default, so the car would be refused. Create a config file, e.g. `mosquitto_is19.conf`:

```
listener 1883 0.0.0.0
allow_anonymous true
```

Run the broker with it (`-v` prints every connect and publish, which helps a lot for debugging):

```sh
# Windows (PowerShell, in C:\Program Files\mosquitto)
.\mosquitto.exe -c C:\path\to\mosquitto_is19.conf -v

# Linux / macOS
mosquitto -c mosquitto_is19.conf -v
```

> On Windows, stop the background "Mosquitto Broker" service first (Services → Mosquitto Broker → Stop). Otherwise port 1883 is already taken.

### 2.3 Open the firewall (Windows)
OR disable firewall :) cause that's what i did

Allow inbound TCP 1883. Run this in an admin PowerShell:

```powershell
New-NetFirewallRule -DisplayName "MQTT 1883" -Direction Inbound -Protocol TCP -LocalPort 1883 -Action Allow
```

Also set the Wi-Fi network to **Private**, not Public.

### 2.4 Find the laptop's IP address

- Windows: `ipconfig`, then the **Wireless LAN adapter Wi-Fi → IPv4 Address** (e.g. `172.20.10.4` on an iPhone hotspot)
- Linux/macOS: `ip addr` / `ipconfig getifaddr en0`

> Use the **Windows** Wi-Fi IP even if you build in WSL. WSL's own `172.x` address is not reachable from the car.
> The laptop's IP can change each time you reconnect to the hotspot. Re-check it before each session.

### 2.5 Test the broker before touching the car

Use two terminals on the laptop:

```sh
mosquitto_sub -h <laptop-ip> -t 'test/#' -v
mosquitto_pub -h <laptop-ip> -t test/hello -m hi
```

If `test/hello hi` appears, the broker works. If it only works with `-h localhost`, recheck 2.2 and 2.3.

---

## 3. Configure the firmware

### 3.1 Create `config/wifi_secrets.h`
Copy the example but just need do some changes

```sh
cd <repo>/config
cp wifi_secrets.example.h wifi_secrets.h     # wifi_secrets.h is gitignored: never commit it
```

Edit `wifi_secrets.h`:

```c
#define WIFI_SSID        "YourHotspotName"     /* 2.4 GHz, case-sensitive */
#define WIFI_PASSWORD    "YourPassword"

#define MQTT_BROKER_IP   "172.20.10.4"         /* laptop IP from step 2.4 */
#define MQTT_BROKER_PORT 1883
#define MQTT_CLIENT_ID   "inf2004-IS19"        /* CHANGE from "mtk3smp-pico" */
#define MQTT_TOPIC       "mtk3smp-pico/heartbeat"   /* template heartbeat topic; IS19 doesn't use it */
```

Rules:
- `MQTT_BROKER_IP` must be a plain **IPv4 address**. The code uses `ip4addr_aton()`, so a hostname like `broker.local` fails with `mqtt: bad broker address`.
- `MQTT_CLIENT_ID` must be **unique on the broker**. Two clients with the same ID keep kicking each other off, and every team's template starts as `mtk3smp-pico`. Your current `wifi_secrets.h` still has `mtk3smp-pico`, so change it.

### 3.2 Topics (already set, nothing to do)

These are defined in `app_program/app_config.h` (section 8):

| Topic | Direction | Content |
|---|---|---|
| `inf2004/IS19/telemetry` | car → | 5 Hz telemetry JSON |
| `inf2004/IS19/status` | car → | 1 Hz heartbeat (link + fault state) |
| `inf2004/IS19/event` | car → | route log, obstacle profile, faults, motion class, run summary |
| `inf2004/IS19/ack` | car → | reply to each command |
| `inf2004/IS19/cmd` | → car | commands (the only topic the car subscribes to) |

To use a different team root, change `CFG_TOPIC_ROOT`.

---

## 4. Build the MQTT image

```sh
cd <repo>/build_make
make -j8 NET=lwip MQTT=1
```

- `NET=lwip` adds TCP/IP. `MQTT=1` adds the MQTT client and `mqtt_bridge.c`. MQTT needs both: `MQTT=1` without `NET=lwip` stops with an error.
- Output: **`build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt.uf2`**
- If you switch build profiles, the makefile deletes the old object files automatically ("Build profile is now ..."). You don't need `make clean`.
- Rebuild every time you change `wifi_secrets.h`. The Wi-Fi details and the first broker are compiled into the image.

---

## 5. Flash and watch the console

1. Hold **BOOTSEL**, plug in the Pico W, then release. An `RPI-RP2` drive appears.
2. Copy the `_mqtt.uf2` onto it. From WSL, `explorer.exe .` opens the folder in Windows.
3. Open the USB serial port at **115200**: PuTTY or the VS Code Serial Monitor on the new COM port (Windows), or `screen /dev/ttyACM0 115200` (Linux). (Prob can just skip)

A good boot looks like this:

```
wifi: cyw43_utk_task starting ...
wifi: joining "YourHotspotName" ...
...                                  (link up, IP assigned)
mqtt: connected to 172.20.10.4
mqtt: subscribed to inf2004/IS19/cmd
```

The Mosquitto `-v` window should also show `New client connected ... as inf2004-IS19`.

---

## 6. See the data and send commands
IF don't want run on MQTT Explorer & want run on terminal. On powershell, run:  
```sh
cd "C:\Program Files\mosquitto"
.\mosquitto_sub.exe -h localhost -t "inf2004/IS19/#" -F "%I %t %p" | Tee-Object -FilePath "WHERE YOU WANT THIS .TXT TO STORE IN YOUR DEVICE"
```
E.g.:
.\mosquitto_sub.exe -h localhost -t "inf2004/IS19/#" -F "%I %t %p" | Tee-Object -FilePath "C:\Users\jolin\OneDrive\Documents\SIT\Y2T1\INF2004 Embedded Sys Prog\Project\car_log.txt"

should work...

### 6.1 Subscribe to everything the car sends

```sh
mosquitto_sub -h <laptop-ip> -t 'inf2004/IS19/#' -v
```

You can also use a GUI: **MQTT Explorer** (<https://mqtt-explorer.com>) or **MQTTX**. Connect to `<laptop-ip>:1883` with no username or password and subscribe to `inf2004/IS19/#`.

You should see `status` once per second and `telemetry` five times per second.

### 6.2 Send commands

Commands are JSON on `inf2004/IS19/cmd`. `seq` is echoed back on `inf2004/IS19/ack`.

```sh
mosquitto_pub -h <laptop-ip> -t inf2004/IS19/cmd -m '{"seq":1,"cmd":"summary"}'
mosquitto_pub -h <laptop-ip> -t inf2004/IS19/cmd -m '{"seq":2,"cmd":"stop"}'
mosquitto_pub -h <laptop-ip> -t inf2004/IS19/cmd -m '{"seq":3,"cmd":"remap","letter":"E","action":"left"}'
mosquitto_pub -h <laptop-ip> -t inf2004/IS19/cmd -m '{"seq":4,"cmd":"default_map"}'
mosquitto_pub -h <laptop-ip> -t inf2004/IS19/cmd -m '{"seq":5,"cmd":"broker","ip":"192.168.1.20","port":1883}'
```

- `remap` actions: `none`, `straight`, `left`, `right`, `uturn`.
- Only flat string and integer values are accepted. There is **no START command**: you start the car with its button.
- Max command length: 128 bytes (`CFG_MQTT_CMD_MAX`).
- `broker` switches to a new broker **without reflashing**. It is acknowledged on the old broker, then applied 300 ms later. It is kept in RAM only, so a reboot goes back to `wifi_secrets.h`.
- If the command queue is full, a `stop` still goes straight to Safety. Other commands get no ack, and you can resend them.

> In Windows PowerShell, quoting JSON is awkward. Use WSL or MQTT Explorer to send commands instead.

---

## 7. How it works inside (for the report or viva)

- `lib/libwifi/.../cyw43_utk.c`: the `cyw43_utk_task` owns the radio and joins `WIFI_SSID`.
- `lib/liblwip/.../mqtt_utk.c`: the lwIP MQTT client. It runs **inside that same task**, connects to the broker, subscribes, and publishes every 10 ms. If the connection drops it reconnects with back-off (1, 2, 4 … up to 30 s).
- `app_program/subsystem1/mqtt_bridge.c`: the hooks the MQTT code calls. They only pass data through message buffers with `TMO_POL`, so they never block:
  - IF-13 `mbf_publish`: Telemetry → MQTT (topic number + JSON)
  - IF-14 `mbf_cmd_in`: MQTT → Command task (command text)
  - IF-15 `mbf_mqtt_cfg`: Command → MQTT (new broker)
  - The link state is the `COMMS_FLG_MQTT_UP` bit in `g_flg_comms`
- No IS19 task calls cyw43, lwIP or MQTT directly.

---

## 8. Troubleshooting

| Symptom (console / broker) | Likely cause | Fix |
|---|---|---|
| Build: `PICO_SDK_PATH is not set` | Shell variable missing | Step 1, open a new terminal |
| Build: lwIP not found / `core/init.c` missing | `lib/lwip` submodule not fetched | `git submodule update --init lib/lwip` in the SDK |
| Build: `MQTT=1 requires NET=lwip` | Missing flag | `make NET=lwip MQTT=1` |
| `wifi: scan done -- 0 network(s)` or the SSID isn't listed | 5 GHz network, or hotspot not 2.4 GHz | Use 2.4 GHz (iPhone: Maximise Compatibility) |
| Wi-Fi joins but never gets an IP | Wrong password / bad hotspot | Recheck `WIFI_PASSWORD`, rebuild |
| `mqtt: bad broker address` | Hostname or typo in `MQTT_BROKER_IP` | Use a dotted IPv4 address |
| `mqtt: connection status ...` then `retrying in N ms`, repeating | Broker unreachable | Broker running with `listener 1883 0.0.0.0`? Firewall open? Laptop IP changed? Same network? |
| Broker log: client connects/disconnects repeatedly | Duplicate `MQTT_CLIENT_ID` | Make it unique, rebuild |
| Connected but nothing in `mosquitto_sub` | Wrong topic filter | Subscribe to `inf2004/IS19/#` (or `#` to see everything) |
| Commands get no ack | Bad JSON, wrong topic, too long, or queue full | Check the console `mqtt: RX [...]` line, use exactly `inf2004/IS19/cmd` |
| Works until a reboot after a `broker` command | `broker` is RAM-only | Expected; put the address in `wifi_secrets.h` for a permanent change |

---

## 9. Quick checklist

- [ ] SDK submodules: `tinyusb`, `cyw43-driver`, **`lwip`**
- [ ] Mosquitto running with `listener 1883 0.0.0.0` + `allow_anonymous true`, firewall open
- [ ] `mosquitto_pub`/`sub` test works using the laptop's **Wi-Fi IP**
- [ ] `config/wifi_secrets.h`: 2.4 GHz SSID/password, broker IP, **unique client ID**
- [ ] `make -j8 NET=lwip MQTT=1` → flash `..._wifi_lwip_mqtt.uf2`
- [ ] Console shows `mqtt: connected` + `subscribed to inf2004/IS19/cmd`
- [ ] `mosquitto_sub -t 'inf2004/IS19/#' -v` shows status + telemetry
- [ ] `{"seq":1,"cmd":"summary"}` gets an ack
