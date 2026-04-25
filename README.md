# FurNFC ASCII (Flipper Zero)

## Introduction

**FurNFC ASCII** is a Flipper Zero application designed to read and decode **Mifare Classic NFC tags** and parse **`.nfc` dump files**, extracting readable ASCII content.

It supports both **live NFC reading** and **.nfc file decoding**, with optional custom Mifare keys and a simple UI for output inspection.

The goal is to quickly visualize human-readable data stored inside NFC memory blocks.

---

## Features

### NFC Reading

* Live scanning of **Mifare Classic tags**
* Supports:
  * Default key: `FF FF FF FF FF FF`
  * Custom user-defined key (hex input)
* Detects tag type (1K / 4K / Mini)
* Supports partial reads

### File Decoding

* Opens and parses `.nfc` dumps
* Validates Flipper NFC format (`Flipper NFC device`)
* Supports only **Mifare Classic dumps**
* Extracts readable ASCII from memory blocks

### ASCII Extraction

* Filters non-printable bytes
* Ignores:

  * `0x00`
  * `0xFF`
  * Sector trailer blocks
* Converts control characters to spaces
* Displays clean ASCII lines only

### UI Features

* Submenu-driven interface
* Popup status view during scanning
* Scrollable text output view
* Help screen
* File picker for `.nfc` files
* Text input for Mifare keys

---

## Project Structure

```
application.fam        # Flipper FAP manifest
furnfc_ascii.c         # Main application source
fapicon.png            # Fap icon
```

---

## Requirements

* Flipper Zero device
* Firmware with FBT (SDK) (e.g. Unleashed / OFW)
---

## Installation

### Option 1: Install prebuilt `.fap`

Copy the compiled file to:

```
ext/apps/nfc/
```

Launch:

```
Applications → NFC → FurNFC
```

---

### Option 2: Build from source

Clone your firmware environment:

```bash
git clone https://github.com/MeiosisKMZ/FurNFC
```

Place the project inside:

```
applications_user/furnfc_ascii/
```

Resulting structure:

```
applications_user/
└── furnfc_ascii/
    ├── application.fam
    ├── fapicon.png
    └── furnfc_ascii.c
```

---

## Build Instructions

From your Flipper Zero firmware root (FBT environment):

```bash
./fbt fap_furnfc_ascii
```

After build completion, output will be located in:

```
build/<firmware>/extapps/
```

Copy `.fap` to your Flipper SD card under:

```
ext/apps/nfc/
```

---

## Usage

### Main Menu Options

When launching the app, you will see options:

#### Read tag

Live NFC scan mode:

* Waits for a Mifare Classic tag
* Tries default key automatically
* Falls back to custom key if set
* Displays decoded ASCII result


#### Mifare Key

* Input custom 6-byte hex key (12 hex characters)
* Example:

  ```
  A0A1A2A3A4A5
  ```

#### Help

* Shows built-in help screen
* Explains decoding behavior and limitations

---

## ASCII Decoding Logic

Each block is processed as follows:

* Skips:

  * `0x00`
  * `0xFF`
* Keeps printable ASCII:

  * `32–126`
* Converts:

  * `\n \r \t → space`
* Ignores sector trailer blocks

Only blocks that contain valid text are displayed.

---

## File Validation

When opening `.nfc` files:

The app checks:

* Header must be:

  ```
  Flipper NFC device
  ```
* Device type must be:

  ```
  Mifare Classic
  ```

Otherwise, decoding is rejected.

---

## Known Limitations

* Only supports **Mifare Classic**
* Cannot decrypt secured sectors without valid keys
* ASCII extraction is heuristic (not structured parsing)
* UTF-8 / Unicode not supported (ASCII only)
* Some data may appear incomplete due to encryption

---

## Troubleshooting

### No tag detected

* Ensure NFC tag is Mifare Classic
* Move tag closer to back of Flipper
* Retry scan

### Empty output

* Data may be encrypted or non-ASCII
* Try custom key
* Try different tag

### File not opening

* Ensure file is in `/ext/nfc/`
* Confirm `.nfc` format is valid Flipper export

### Partial read warning

* Some sectors could not be decrypted
* Try custom key for better results

---

## Contributing

Contributions are welcome !

## License

Licensed under the project [LICENSE](LICENSE) file.

> TL;DR Keep my code available for everyone and free. Credit me when you use my code, other than that you are free to use it !
