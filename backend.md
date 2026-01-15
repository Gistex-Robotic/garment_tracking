# Garment Tracking Backend API

Dokumentasi ini menjelaskan Backend API untuk sistem **Garment Tracking** yang digunakan dalam proses produksi (sewing, dryroom, folding, finishing, dan reject).

---

## Base URL

| Environment | Base URL                 |
| ----------- | ------------------------ |
| CLN         | `http://10.8.0.104:7000` |
| MAJA        | `http://10.5.0.106:7000` |

---

## Authentication

Semua endpoint **wajib** menggunakan header berikut:

```
X-Api-Key: <hubungi admin>
```

---

## User API

### GET /user

Mengambil data user berdasarkan RFID / ID Card.

**Query Parameter**

| Parameter | Type   | Deskripsi               |
| --------- | ------ | ----------------------- |
| rfid_user | string | RFID / ID Card karyawan |

**Response Example**

```json
{
  "success": true,
  "password_hash": "XXX",
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

## Work Order (WO)

### GET /wo/branch

Mengambil data WO Schedule dari server GCC.

**Query Parameter**

| Parameter       | Type   | Deskripsi                         |
| --------------- | ------ | --------------------------------- |
| branch          | string | Kode branch (CJL, dll)            |
| line            | string | Line produksi                     |
| start_date_from | date   | Filter tanggal mulai (YYYY-MM-DD) |

---

## Card Status

### GET /card

Menampilkan status kartu garment.

**Response**

```json
{
  "success": true,
  "data": [{
    "total_cards": 201,
    "done": 16,
    "progress": 155,
    "waiting": 30
  }]
}
```

---

## Monitoring

### GET /monitoring/line

Menampilkan data tracking terakhir per line (hari ini).

**Query Parameter**

| Parameter | Type   |
| --------- | ------ |
| line      | string |

---

## Garment

### GET /garment/waiting

Menampilkan garment yang belum diproses (waiting).

### POST /garment/update

Update data garment yang sudah diregistrasi namun belum diproses.

**Request Body**

```json
{
  "rfid_garment": "XXX",
  "item": "XXX",
  "style": "XXX",
  "wo": "XXX",
  "color": "XXX",
  "size": "XXX"
}
```

---

## WIRA & Produksi

### GET /wira

Menampilkan akumulasi OUTPUT, REWORK, REJECT, GOOD, dan WIRA.

**Filter Opsional**

* `tanggalfrom`
* `tanggalto`
* `line`
* `wo`

---

## Tracking

### GET /tracking/check

Cek status garment yang belum done dan belum move.

---

## Input RFID

### POST /inputRFID

Input RFID garment baru.

**Catatan**

* Duplikasi RFID yang masih aktif akan mengembalikan **HTTP 400**

---

## User Management

### POST /inputUser

Menambahkan user baru.

**Validasi**

* Field wajib: `rfid_user`, `password`, `nama`, `nik`, `bagian`, `line`
* Duplikasi NIK → **HTTP 409**

---

## Scrap

### POST /scrap

Menandai garment sebagai reject mati (SCRAP).

---

## Finishing Summary

### GET /finishing

Menampilkan ringkasan:

* Dryroom
* Folding
* Reject Room

---

## Dryroom

### POST /garment/dryroom/in

Input garment masuk dryroom.

### POST /garment/dryroom/out

Input garment keluar dryroom.

### GET /garment/dryroom/count

Rekap waiting, check-in, dan check-out dryroom.

### GET /garment/dryroom/hourly

Rekap IN & OUT dryroom per jam.

---

## Folding

### POST /garment/folding/in

Input garment masuk folding.

### POST /garment/folding/out

Input garment keluar folding.

### GET /garment/folding/count

Rekap waiting, check-in, dan check-out folding.

### GET /garment/folding/hourly

Rekap IN & OUT folding per jam.

---

## Catatan Teknis

* Semua timestamp menggunakan server time
* Data historis dipindahkan ke `tracking_movement_end`
* Pastikan urutan proses: **Sewing → PQC → Dryroom → Folding → Finishing**

---

📌 **Repository**: [https://github.com/Gistex-Robotic/garment_tracking](https://github.com/Gistex-Robotic/garment_tracking)
