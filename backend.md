# Backend RFID API Documentation

## Base URL

```
http://10.8.0.104:7000
```

---

## Authentication

Saat ini endpoint **tidak menggunakan authentication** (public / internal network).

---

## Endpoints Overview

* User
* Work Order (WO)
* Card Status
* Line Monitoring
* WIRA Report
* Tracking Check
* Input RFID Garment
* Input User
* Scrap Garment

---

## 1. Get User by RFID

### Endpoint

```
GET /user
```

### Query Parameters

| Parameter | Type   | Description             |
| --------- | ------ | ----------------------- |
| rfid_user | string | RFID / ID Card karyawan |

### Example Request

```
/user?rfid_user=XXXXX
```

### Response

```json
{
  "password_hash": "XXX",
  "success": true,
  "user": {
    "nik": "XXX",
    "nama": "XXX",
    "bagian": "XXX",
    "line": "XXX",
    "rfid_user": "XXX",
    "telegram": "XXX",
    "no_hp": "XXX",
    "pwd_md5": "XXX"
  }
}
```

---

## 2. Work Order (WO)

### Endpoint

```
GET /wo/branch
```

### Query Parameters

| Parameter       | Type                | Description          |
| --------------- | ------------------- | -------------------- |
| branch          | string              | Kode branch          |
| line            | string              | Line produksi        |
| start_date_from | string (YYYY-MM-DD) | Filter tanggal mulai |

### Example Requests

```
/wo/branch?branch=CJL
/wo/branch?branch=CJL&line=L1
/wo/branch?branch=CJL&line=L1&start_date_from=2025-12-01
```

### Response

```json
{
  "branch": "CJL",
  "count": 93,
  "line": "L1",
  "date_from": "2025-12-01",
  "date_to": null,
  "success": true,
  "data": [
    {
      "id": 21226,
      "wo_no": "1886",
      "style": "611",
      "buyer": "XXX",
      "product_name": "T-Shirt",
      "color": "ROYAL BLUE",
      "size": "34",
      "qty": "10.00",
      "line": "L1",
      "branch": "CJL",
      "start_date": "Fri, 05 Dec 2025 00:00:00 GMT",
      "finish_date": "Fri, 05 Dec 2025 00:00:00 GMT"
    }
  ]
}
```

---

## 3. Card Status Summary

### Endpoint

```
GET /card
```

### Description

Menampilkan status kartu RFID garment:

* total_cards
* progress
* waiting
* done

### Response

```json
{
  "success": true,
  "data": [
    {
      "total_cards": 201,
      "done": "16",
      "progress": "155",
      "waiting": "30"
    }
  ]
}
```

---

## 4. Line Monitoring

### Endpoint

```
GET /monitoring/line
```

### Query Parameters

| Parameter | Type   | Description   |
| --------- | ------ | ------------- |
| line      | string | Line produksi |

### Example Request

```
/monitoring/line?line=1
```

### Response

```json
{
  "success": true,
  "line": "1",
  "data": {
    "buyer": "XXX",
    "wo": "1857",
    "style": "16647",
    "item": "OUTER JACKET",
    "color": "TN25",
    "size": "M",
    "line": "1",
    "rfid_garment": "000"
  }
}
```

---

## 5. WIRA Report

### Endpoint

```
GET /wira
```

### Query Parameters

| Parameter   | Type                | Description   |
| ----------- | ------------------- | ------------- |
| tanggalfrom | string (YYYY-MM-DD) | Tanggal mulai |
| tanggalto   | string (YYYY-MM-DD) | Tanggal akhir |
| line        | string              | Line produksi |
| wo          | string              | Nomor WO      |

### Example Requests

```
/wira
/wira?tanggalfrom=2025-12-01&tanggalto=2025-12-05
/wira?line=1&wo=185759
```

### Response

```json
{
  "success": true,
  "filters": {
    "line": null,
    "wo": null,
    "tanggalfrom": "2025-12-01",
    "tanggalto": "2025-12-05"
  },
  "data": [
    {
      "line": "1",
      "WO": "1859",
      "Style": "11047",
      "Item": "OUTER JACKET",
      "Buyer": "XXX",
      "Output Sewing": "204",
      "Rework": "57",
      "Reject": "2",
      "Good": "174",
      "PQC Rework": "56",
      "PQC Reject": "0",
      "PQC Good": "167",
      "WIRA": 8,
      "PQC WIRA": 27,
      "Balance": "-37"
    }
  ]
}
```

---

## 6. Tracking Check

### Endpoint

```
GET /tracking/check
```

### Query Parameters

| Parameter    | Type   | Description  |
| ------------ | ------ | ------------ |
| rfid_garment | string | RFID garment |

### Example Request

```
/tracking/check?rfid_garment=XXX
```

### Response

```json
{
  "success": true,
  "message": "Data ditemukan",
  "garment": {
    "id_garment": 278,
    "rfid_garment": "0052",
    "wo": "1859",
    "style": "1107",
    "buyer": "XXX",
    "item": "OUTER JACKET",
    "color": "TN25",
    "size": "M",
    "isDone": "",
    "isMove": "",
    "timestamp": "Thu, 04 Dec 2025 13:35:33 GMT"
  }
}
```

---

## 7. Input RFID Garment

### Endpoint

```
POST /inputRFID
```

### Headers

```
Content-Type: application/json
```

### Request Body

```json
{
  "rfid_garment": "TEST-INPUT",
  "wo": "WOWOWO",
  "style": "ST",
  "buyer": "BY",
  "item": "IT",
  "color": "CO",
  "size": "SZ"
}
```

### Notes

* Duplikasi `rfid_garment` aktif → **400 Bad Request**
* Success → **200 OK**

---

## 8. Input User

### Endpoint

```
POST /inputUser
```

### Request Body

```json
{
  "rfid_user": "TEST",
  "password": "12345",
  "nama": "XXX",
  "nik": "5555",
  "bagian": "SEWING",
  "line": "LINE 5",
  "telegram": "",
  "no_hp": ""
}
```

### Notes

* Field wajib kosong → **400 Bad Request**
* NIK duplikat → **409 Conflict**

---

## 9. Scrap Garment

### Endpoint

```
POST /scrap
```

### Request Body

```json
{
  "rfid_garment": "XXXXX"
}
```

### Description

Menandai garment sebagai **reject mati** dan set `isDone = done`.

---

## Status Codes

| Code | Description |
| ---- | ----------- |
| 200  | Success     |
| 400  | Bad Request |
| 409  | Conflict    |

---

## Maintainer

Backend RFID System – Internal Garment Production
