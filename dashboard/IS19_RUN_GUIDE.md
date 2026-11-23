# IS19 Pico Car – Wi‑Fi Telemetry, Mosquitto and Dashboard Run Guide

Your setup:

| What | Where |
|---|---|
| Firmware project (WSL) | `~/is19/INF2004_IS19-mainv4` (in Windows: `\\wsl.localhost\Ubuntu-24.04\home\dominic\is19\INF2004_IS19-mainv4`) |
| Build folder | `~/is19/INF2004_IS19-mainv4/build_make` |
| Wi‑Fi and broker settings | `config/wifi_secrets.h` (**never commit this file**) |
| Mosquitto config | `C:\mosquitto\mosquitto_is19.conf` |
| Dashboard | `IS19_car_dashboard.html` (open it in Chrome or Edge) |

How it fits together:

```
 Pico W car  --(Wi-Fi, MQTT :1883)-->  Mosquitto on your laptop  --(WebSocket :9001)-->  Dashboard in browser
             <-- commands (start/stop/reset) --------------------------------------------- buttons
```

---

## 1. One-time checks (skip if already done)

In **WSL (Ubuntu)**:

```bash
# SDK path must be set (should already be in ~/.bashrc)
echo $PICO_SDK_PATH                 # expect /home/dominic/pico/pico-sdk

# lwIP must be present for the MQTT build
ls $PICO_SDK_PATH/lib/lwip/src/core/init.c
# If "No such file":
cd ~/pico/pico-sdk && git submodule update --init lib/lwip
```

**Wi‑Fi secrets file**: `config/wifi_secrets.h` must contain:

```c
#define WIFI_SSID        "your hotspot name"      // 2.4 GHz only
#define WIFI_PASSWORD    "your hotspot password"
#define MQTT_BROKER_IP   "x.x.x.x"                // your LAPTOP's IP on the hotspot (see step 4)
#define MQTT_BROKER_PORT 1883
```

- iPhone hotspot: turn on **Maximise Compatibility** (forces 2.4 GHz).
- If the file is missing, copy `config/wifi_secrets.example.h` to `config/wifi_secrets.h` and fill it in.

---

## 2. Build the firmware

In **WSL**:

```bash
cd ~/is19/INF2004_IS19-mainv4/build_make
make -j8 NET=lwip MQTT=1
```

Output: `build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt.uf2`

If you switch between build types (e.g. after a `WIFI=none` or `BENCH=1` build), or the build behaves oddly, clean first:

```bash
make clean NET=lwip MQTT=1
make -j8 NET=lwip MQTT=1
```

Other builds (not for telemetry):

| Purpose | Command | Output `.uf2` |
|---|---|---|
| Full car + Wi‑Fi + MQTT (**demo**) | `make -j8 NET=lwip MQTT=1` | `mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt.uf2` |
| Car without Wi‑Fi | `make -j8 WIFI=none` | `mtk3pico_smp1_usb_cdc.uf2` |
| Motor / encoder bench only | `make -j8 BENCH=1 WIFI=none` | `mtk3pico_smp1_usb_cdc_bench.uf2` |

---

## 3. Flash the car

1. Unplug the Pico. **Hold the BOOTSEL button**, plug the USB cable in, then release.
2. A drive called **RPI-RP2** appears in Windows.
3. Open the build folder in Windows Explorer from WSL:
   ```bash
   cd ~/is19/INF2004_IS19-mainv4/build_make
   explorer.exe .
   ```
4. Drag `mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt.uf2` onto **RPI-RP2**. The Pico reboots by itself.

---

## 4. Find your laptop's IP (the broker IP)

1. Connect the **laptop** to the same hotspot as the car.
2. In **PowerShell**:
   ```powershell
   ipconfig
   ```
3. Under **Wireless LAN adapter Wi‑Fi**, read **IPv4 Address** (e.g. `172.20.10.4`).
4. That must match `MQTT_BROKER_IP` in `config/wifi_secrets.h`. If it changed, edit the file and **rebuild + reflash** (steps 2–3).

---

## 5. Start Mosquitto

`C:\mosquitto\mosquitto_is19.conf` should contain:

```
# Car connects here (plain MQTT)
listener 1883 0.0.0.0
# Dashboard (browser) connects here (MQTT over WebSockets)
listener 9001
protocol websockets
allow_anonymous true
```

In **PowerShell (Run as Administrator)**:

```powershell
# 1) Stop the Mosquitto Windows service if it is running (it blocks port 1883)
net stop mosquitto

# 2) Run Mosquitto with the IS19 config
& "C:\Program Files\mosquitto\mosquitto.exe" -c C:\mosquitto\mosquitto_is19.conf -v
```

> If you installed Mosquitto into `C:\mosquitto` instead, use
> `& "C:\mosquitto\mosquitto.exe" -c C:\mosquitto\mosquitto_is19.conf -v`

You should see:

```
Opening ipv4 listen socket on port 1883.
Opening websockets listen socket on port 9001.
```

Leave this window open. When the car connects you'll see `New client connected from 172.20.10.x ...`.

**Firewall (one time only, admin PowerShell)**, so the car can reach the laptop:

```powershell
New-NetFirewallRule -DisplayName "Mosquitto 1883" -Direction Inbound -Protocol TCP -LocalPort 1883 -Action Allow
```

---

## 6. Open the dashboard

1. Double‑click `IS19_car_dashboard.html` (Chrome or Edge).
2. Broker box: `ws://localhost:9001`. Topic root: `inf2004/IS19`.
3. Click **Connect**. The status should turn green and the heartbeat starts ticking once the car is online.

Buttons:

| Button | What it does |
|---|---|
| **START** | Starts the run (same as the car's START button GP20) |
| **STOP** | Stops the car |
| **RESET** | Clears SAFE HOLD / fault (same as the RESET button GP21). Then press START again |
| Request summary | Car sends its run summary |
| Remap / Default letter map | Change what a barcode letter does |
| Export | Saves the event log as a `.txt` (useful for debugging, e.g. hump `peak` values) |

---

## 7. Every‑session run order (checklist)

1. ☐ Phone hotspot ON (Maximise Compatibility), laptop connected to it
2. ☐ `ipconfig` → laptop IP = `MQTT_BROKER_IP` (if not: edit, rebuild, reflash)
3. ☐ Admin PowerShell → `net stop mosquitto` → run Mosquitto with `-c C:\mosquitto\mosquitto_is19.conf -v`
4. ☐ Open the dashboard → **Connect** to `ws://localhost:9001`
5. ☐ Power the car, place it on the line and keep it **still** for about 2 s (IMU calibration)
6. ☐ Wait for the car to show up in Mosquitto's log and the dashboard heartbeat
7. ☐ Press **START** (dashboard or GP20)
8. ☐ If it goes to **SAFE HOLD**: read the `hold` / `cause` on the dashboard → press **RESET** → **START**

---

## 8. Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Build: `lwIP not found` | lwIP submodule missing | `cd ~/pico/pico-sdk && git submodule update --init lib/lwip` |
| Build: `wifi_secrets.h: No such file` | secrets file missing | copy `wifi_secrets.example.h` → `wifi_secrets.h`, fill in |
| Mosquitto: `Address already in use` | Windows service already on 1883 | `net stop mosquitto` (admin) |
| Car never appears in Mosquitto log | wrong broker IP / 5 GHz / firewall | check `ipconfig`, Maximise Compatibility, firewall rule |
| Dashboard won't connect | Mosquitto not running with the conf, or no `listener 9001` | restart Mosquitto with `-c ...conf`, check "websockets listen socket on port 9001" |
| Dashboard connected but no data | car not connected yet / wrong topic root | topic root must be `inf2004/IS19` |
| Fault `cause 5 from 3` at boot | queue full while Wi‑Fi starts | press **RESET** then **START** |
| SAFE HOLD `no_command` | junction reached with no barcode read | barcode sensor issue (see barcode notes); RESET → START |
| Hump height too big | uncalibrated hump table | adjust `CFG_IMU_HUMP_TABLE_MM` in `app_program/app_config.h` |

---

## 9. Don't commit secrets

`config/wifi_secrets.h` is in `.gitignore`. Before pushing, check:

```bash
cd ~/is19/INF2004_IS19-mainv4
git status          # wifi_secrets.h must NOT be listed
```

Optional clean‑up of Windows download tags (the `...Zone.Identifier` files) in WSL:

```bash
find ~/is19/INF2004_IS19-mainv4 -name "*Zone.Identifier" -delete
```
