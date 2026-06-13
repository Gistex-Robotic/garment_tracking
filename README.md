# Garment Tracking RFID IN/OUT ESP32

Program ieu dipaké pikeun alat RFID basis **ESP32** dina sistem garment tracking prosés **IN/OUT**.
Alat maca kartu RFID user atawa RFID bundle, ngirim data ka MQTT, nampa response ti backend, terus nembongkeun status prosés dina LCD 16x2.

Program utama:

```text
in_out.cpp
```

---

## 1. Fungsi Utama

Program ESP32 ieu boga fungsi pikeun:

* Login operator maké kartu RFID user.
* Nangtukeun alat salaku alat **IN** atawa **OUT**.
* Maca kartu RFID bundle.
* Ngirim data scan ka MQTT broker.
* Nampa response ti backend ngaliwatan MQTT.
* Nembongkeun status dina LCD 16x2.
* Malikkeun deui session login sanggeus ESP restart.
* Nembongkeun total output jeung jumlah bundle dina mode idle.
* Mere notifikasi sora maké passive buzzer.
* Nyadiakeun tombol optional pikeun pesen husus.

---

## 2. Alur Sistem

```text
ESP32 RFID
   ↓ MQTT Publish
Node-RED
   ↓ HTTP POST
Backend PHP scan.php
   ↓ MySQL
Database Garment Tracking
   ↓ JSON Response
Node-RED
   ↓ MQTT Reply
ESP32 LCD
```

ESP32 henteu langsung ngakses database.
ESP32 ngan ngirim data RFID ka MQTT jeung nampa balasan ti backend.

---

## 3. Identitas Alat IN / OUT

Identitas alat ditangtukeun tina variable:

```cpp
String scanType = "in";
```

atawa:

```cpp
String scanType = "out";
```

Paké:

```cpp
String scanType = "in";
```

pikeun alat **IN**.

Paké:

```cpp
String scanType = "out";
```

pikeun alat **OUT**.

Unggal RFID discan, ESP32 bakal ngirim payload:

```text
RFID/MAC/scan_type
```

Conto alat IN:

```text
0015117752/B4BFE914A440/in
```

Conto alat OUT:

```text
0015117752/B4BFE914A441/out
```

---

## 4. Tampilan LCD Idle

Tampilan idle dina LCD:

```text
OUTPUT | NAMA
C<count> | L<line> | IN/OUT
```

Conto:

```text
150 | RANGGA
C12 | L3 | OUT
```

Katerangan:

| Tampilan | Hartina          |
| -------- | ---------------- |
| `150`    | Total output qty |
| `RANGGA` | Ngaran operator  |
| `C12`    | Jumlah bundle    |
| `L3`     | Line produksi    |
| `OUT`    | Tipe alat        |

---

## 5. Hardware Nu Dipaké

| Komponen         | Fungsi                 |
| ---------------- | ---------------------- |
| ESP32            | Controller utama       |
| RFID Reader UART | Maca kartu RFID        |
| LCD 16x2 I2C     | Nembongkeun status     |
| Passive Buzzer   | Notifikasi sora        |
| Push Button      | Tombol optional        |
| WiFi             | Koneksi ka MQTT broker |

---

## 6. Wiring

| Modul          | Pin Modul | Pin ESP32 |
| -------------- | --------: | --------: |
| RFID Reader    |        TX |    GPIO16 |
| RFID Reader    |        RX |    GPIO17 |
| LCD I2C        |       SDA |    GPIO21 |
| LCD I2C        |       SCL |    GPIO22 |
| Buzzer Passive |    Signal |    GPIO25 |
| Button         |    Signal |    GPIO15 |
| Button         |       GND |       GND |

Catetan:

* LCD maké alamat I2C `0x27`.
* Button maké mode `INPUT_PULLUP`.
* Ulah mencét button nalika ESP keur booting, sabab GPIO15 bisa mangaruhan proses boot dina sababaraha board ESP32.

---

## 7. Library Nu Diperyogikeun

Program ngagunakeun library:

```cpp
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "esp_wifi.h"
#include "esp_bt.h"
```

Library Arduino anu kudu dipasang:

* `LiquidCrystal_I2C`
* `PubSubClient`
* ESP32 Arduino Core

---

## 8. Konfigurasi WiFi jeung MQTT

Robah konfigurasi ieu luyu jeung jaringan produksi:

```cpp
const char* ssid = "NAMA_WIFI";
const char* password = "PASSWORD_WIFI";

const char* mqtt_server = "IP_MQTT_BROKER";
const int mqtt_port = 1883;
```

Conto:

```cpp
const char* mqtt_server = "10.5.0.106";
const int mqtt_port = 1883;
```

Catetan kaamanan:

* Ulah nyimpen password WiFi produksi dina repository public.
* Gunakeun config misah lamun repository bisa diakses umum.
* Pastikeun IP MQTT broker bisa kahontal ku ESP32.

---

## 9. Optimasi WiFi

Program ngagunakeun optimasi WiFi:

```cpp
WiFi.mode(WIFI_STA);
WiFi.setSleep(false);
WiFi.setAutoReconnect(true);
WiFi.persistent(false);
esp_wifi_set_ps(WIFI_PS_NONE);
btStop();
```

Fungsina:

* ESP32 ngan jalan salaku station WiFi.
* WiFi sleep dipareuman supaya MQTT leuwih responsif.
* Auto reconnect WiFi diaktipkeun.
* Persistent WiFi dipareuman supaya konfigurasi teu sering ditulis ka flash.
* Power save WiFi dipareuman pikeun ngurangan delay.
* Bluetooth dipareuman sabab teu dipaké.

---

## 10. Topic MQTT

### 10.1 Topic Kirim RFID

ESP32 publish data scan ka topic:

```text
rfid/batch/<MAC>/tag
```

Conto:

```text
rfid/batch/B4BFE914A440/tag
```

Payload:

```text
RFID/MAC/scan_type
```

Conto:

```text
0015117752/B4BFE914A440/out
```

---

### 10.2 Topic Restore Session

Nalika ESP restart, data RAM bakal balik deui ka default.
Supaya status login henteu leungit, ESP32 ngirim payload restore:

```text
__RESTORE__/MAC/scan_type
```

Conto:

```text
__RESTORE__/B4BFE914A440/out
```

Backend kudu maca session dumasar kana:

```text
mac + scan_type
```

---

### 10.3 Topic Reply ti Backend

ESP32 subscribe topic:

```text
rfid/batch/reply/<MAC>
```

Conto:

```text
rfid/batch/reply/B4BFE914A440
```

Conto payload reply:

```json
{
  "status": "OK",
  "mode": "SCAN_OK",
  "lcd1": "KELUAR OK 15PCS",
  "lcd2": "TOTAL 150PCS",
  "beep": 1,
  "operator_name": "RANGGA",
  "line_no": 3,
  "actual_output": 150,
  "count_bundle": 12,
  "scan_type": "out"
}
```

---

### 10.4 Topic Status Online / Offline

ESP32 publish status ka topic:

```text
rfid/batch/<MAC>/status
```

Payload nalika online:

```text
online
```

Lamun koneksi MQTT pegat, last will message bakal jadi:

```text
offline
```

---

## 11. Format Payload ka Backend

Node-RED kudu ngarobah payload ti ESP32 jadi JSON.

Payload ti ESP32:

```text
RFID/MAC/scan_type
```

Conto:

```text
0015117752/B4BFE914A440/out
```

JSON ka backend:

```json
{
  "mac": "B4BFE914A440",
  "rfid": "0015117752",
  "scan_type": "out"
}
```

Pikeun restore session:

```json
{
  "mac": "B4BFE914A440",
  "rfid": "__RESTORE__",
  "scan_type": "out"
}
```

---

## 12. Format Response Backend

Backend disarankeun ngirim response sapertos ieu:

```json
{
  "status": "OK",
  "mode": "SCAN_OK",
  "mac": "B4BFE914A440",
  "reply_topic": "rfid/batch/reply/B4BFE914A440",

  "lcd1": "SCAN OK",
  "lcd2": "TOTAL 150PCS",
  "beep": 1,

  "scan_type": "out",
  "action": "output_batch",

  "operator_name": "RANGGA",
  "nama": "RANGGA",

  "line_no": 3,

  "actual_output": 150,
  "total_output_qty": 150,
  "operator_output": 150,

  "count_bundle": 12,
  "bundle_count": 12,
  "total_bundle": 12
}
```

Field anu dibaca ku ESP32:

| Field                                                    | Fungsi                |
| -------------------------------------------------------- | --------------------- |
| `lcd1`                                                   | Baris 1 LCD sementara |
| `lcd2`                                                   | Baris 2 LCD sementara |
| `beep`                                                   | Kode buzzer           |
| `operator_name` / `nama`                                 | Ngaran operator       |
| `line_no` / `line`                                       | Line produksi         |
| `actual_output` / `total_output_qty` / `operator_output` | Total output          |
| `count_bundle` / `bundle_count` / `total_bundle`         | Jumlah bundle         |
| `action`                                                 | Status prosés         |

---

## 13. Cara Kerja Booting

Nalika ESP32 hurung:

1. Serial monitor aktif.
2. `scanType` dirobah jadi lowercase.
3. Pin button diset `INPUT_PULLUP`.
4. Buzzer diinisialisasi.
5. RFID UART diinisialisasi.
6. LCD diinisialisasi.
7. ESP32 nyambung ka WiFi.
8. ESP32 nyokot MAC address.
9. Topic MQTT dijieun dumasar MAC address.
10. ESP32 nyambung ka MQTT broker.
11. ESP32 subscribe topic reply.
12. ESP32 publish status `online`.
13. ESP32 ngirim restore session.

---

## 14. Cara Kerja Scan RFID

Nalika kartu RFID kabaca:

1. ESP32 maca data RFID tina UART.
2. Data RFID dirobah jadi format decimal 10 digit.
3. ESP32 mariksa debounce.
4. ESP32 nyieun payload:

```text
RFID/MAC/scan_type
```

5. ESP32 publish ka topic:

```text
rfid/batch/<MAC>/tag
```

6. LCD nembongkeun:

```text
PROCESSING
<RFID>
```

7. ESP32 nungguan response ti backend.

---

## 15. Cara Kerja Restore Session

Nalika ESP restart, data ieu balik deui ka default:

```cpp
operatorName = "NO LOGIN";
actualOutput = 0;
countBundle = 0;
lineNo = 0;
```

Ku sabab éta ESP32 ngirim:

```text
__RESTORE__/MAC/scan_type
```

Lamun backend manggihan session aktif, backend ngirim deui:

* ngaran operator
* line
* total output
* count bundle

Sanggeus response ditarima, LCD idle bakal nembongkeun deui data operator aktif.

---

## 16. Debounce jeung Timeout

Program ngagunakeun debounce RFID:

```cpp
const unsigned long debounceTime = 1500;
```

Hartina kartu anu sarua moal diproses deui dina waktu 1,5 detik.

Program ogé boga timeout proses RFID:

```cpp
const unsigned long rfidProcessingTimeout = 7000;
```

Lamun backend teu ngabales dina 7 detik, LCD nembongkeun:

```text
TIMEOUT
SCAN ULANG
```

---

## 17. Kode Buzzer

|             Beep | Hartina                         |
| ---------------: | ------------------------------- |
|              `0` | Teu aya sora                    |
|              `1` | Notifikasi sukses pondok        |
|              `2` | Dua beep, biasana logout / info |
| `3` atawa leuwih | Error / beep panjang            |

---

## 18. Button Optional

Lamun button diteken, LCD nembongkeun:

```text
BELUM DIPROGRAM
 BANGG!!!
```

Sanggeus sababaraha detik, tampilan balik deui ka idle.

---

## 19. Backend jeung Database

Program ieu biasana dipaké babarengan jeung backend PHP sarta database MySQL.

Tabel anu umum dipaké:

| Tabel                  | Fungsi                                            |
| ---------------------- | ------------------------------------------------- |
| `prep_operator_access` | Nangtukeun user bisa login salaku IN atawa OUT    |
| `session_prep`         | Nyimpen session login dumasar MAC jeung scan_type |
| `v_batch_bundles_long` | Sumber data RFID bundle                           |
| `tracking_prep`        | Nyimpen hasil scan IN/OUT                         |
| `prep_rfid_devices`    | Master data alat RFID                             |

---

## 20. Rule Login Operator

Alat IN ngan narima user anu boga:

```text
prep_operator_access.scan_type = in
```

Alat OUT ngan narima user anu boga:

```text
prep_operator_access.scan_type = out
```

Hiji jalma bisa boga akses IN jeung OUT lamun boga dua row:

```text
rfid_user | scan_type
001234    | in
001234    | out
```

Session disimpen dumasar:

```text
mac + scan_type
```

---

## 21. Rule Scan IN

Syarat scan IN:

1. Alat geus login.
2. RFID bundle kapanggih.
3. Line operator sarua jeung line bundle.
4. Batch operator sarua jeung batch bundle.
5. Bundle can pernah scan IN.

Lamun valid, backend insert:

```text
status = in
rfid_user = user operator IN
mac = MAC alat IN
qty = qty bundle
```

---

## 22. Rule Scan OUT

Syarat scan OUT:

1. Alat OUT geus login.
2. RFID bundle kapanggih.
3. Bundle geus pernah scan IN.
4. Bundle can pernah scan OUT.

Lamun valid, backend insert:

```text
status = out
rfid_user = user operator OUT
mac = MAC alat OUT
qty = qty tina row IN
```

Output jeung count bundle OUT kudu diitung dumasar user operator OUT anu keur login.

---

## 23. Debug ESP32

Paké Serial Monitor baudrate:

```text
115200
```

Output normal:

```text
Device ID : B4BFE914A440
Scan Type : out
topicTag    : rfid/batch/B4BFE914A440/tag
topicStatus : rfid/batch/B4BFE914A440/status
topicReply  : rfid/batch/reply/B4BFE914A440
MQTT Connected
Restore session request: __RESTORE__/B4BFE914A440/out
```

Nalika scan kartu:

```text
Publish : 0015117752/B4BFE914A440/out
```

Nalika narima reply:

```text
RX : rfid/batch/reply/B4BFE914A440 = {...}
```

---

## 24. Debug Node-RED

Aktipkeun debug dina flow:

* Raw RFID payload
* API request
* API response
* Reply to ESP

Pastikeun payload ti ESP:

```text
RFID/MAC/scan_type
```

Pastikeun JSON ka backend:

```json
{
  "mac": "B4BFE914A440",
  "rfid": "0015117752",
  "scan_type": "out"
}
```

Pastikeun response ka ESP boga:

```json
{
  "actual_output": 150,
  "count_bundle": 12,
  "line_no": 3,
  "operator_name": "RANGGA"
}
```

---

## 25. Troubleshooting

### LCD balik deui ka 0 sanggeus restart

Kamungkinan:

* Session teu aya di `session_prep`.
* MAC di database teu sarua jeung MAC ESP.
* `scan_type` session teu sarua jeung alat.
* ESP teu ngirim `__RESTORE__`.
* Backend teu nangani restore session.
* Node-RED teu neruskeun response ka topic reply.

Cek Serial Monitor:

```text
Restore session request: __RESTORE__/MAC/scan_type
```

---

### OUT tetep 0

Kamungkinan:

* Row `tracking_prep` status `out` kasimpen ku `rfid_user` anu salah.
* Backend can ngirim `actual_output`.
* Backend can ngirim `count_bundle`.
* Node-RED teu neruskeun field `count_bundle`.
* ESP teu narima response MQTT.

Query debug:

```sql
SELECT
    rfid_user,
    status,
    COUNT(*) AS count_bundle,
    SUM(qty) AS total_output
FROM tracking_prep
GROUP BY rfid_user, status;
```

---

### Muncul `SCAN TYPE ERR`

Pastikeun variable ieu ngan eusina `in` atawa `out`:

```cpp
String scanType = "out";
```

---

### Muncul `TIMEOUT`

Kamungkinan:

* Backend teu ngabales.
* Node-RED error.
* MQTT reply topic salah.
* Broker MQTT pegat.
* PHP error.

Cek:

* Serial Monitor ESP32
* Debug Node-RED
* Log PHP / XAMPP
* Broker MQTT

---

### Kartu user ditolak

Cek akses user:

```sql
SELECT *
FROM prep_operator_access
WHERE rfid_user = 'RFID_USER';
```

Pikeun alat OUT, user kudu boga:

```text
scan_type = out
```

Pikeun alat IN, user kudu boga:

```text
scan_type = in
```

---

## 26. Checklist Deploy Alat Anyar

1. Upload firmware ESP32.
2. Set `scanType` luyu jeung fungsi alat.
3. Pastikeun WiFi bener.
4. Pastikeun IP MQTT broker bener.
5. Catet MAC address tina Serial Monitor.
6. Pastikeun Node-RED aktif.
7. Pastikeun backend `scan.php` aktif.
8. Pastikeun database aktif.
9. Pastikeun operator geus kadaptar di `prep_operator_access`.
10. Tap kartu user pikeun login.
11. Tap kartu bundle pikeun test IN/OUT.
12. Cek data asup ka `tracking_prep`.
13. Cek tampilan idle LCD.

---

## 27. Catetan Penting

* Ulah retain topic reply MQTT.
* Retained reply lami bisa nyababkeun ESP maca response lami nalika boot.
* Topic reply kudu salawasna saluyu jeung MAC ESP.
* Hiji alat ngan boga hiji identitas: `in` atawa `out`.
* Lamun identitas alat ditangtukeun tina program ESP, ulah ditimpa ku `scan_type` tina Node-RED retained config.
* Paké IP MQTT broker anu stabil.
* Ulah nyimpen password WiFi produksi dina repository public.

---

## 28. Lisensi

Internal project pikeun kabutuhan garment tracking jeung otomasi produksi.
