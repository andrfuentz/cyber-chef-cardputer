# Install and flashing guide

This guide explains how to flash MiseDeck to an M5Stack Cardputer.

## Requirements

- M5Stack Cardputer or Cardputer ADV
- USB-C data cable
- Computer with PlatformIO
- microSD card formatted as FAT32

## Build with PlatformIO

From the firmware project folder:

```bash
pio run
```

To upload:

```bash
pio run -t upload
```

If multiple serial devices are connected, specify the port:

```bash
pio run -t upload --upload-port COM3
```

On macOS/Linux, the port will look different, such as `/dev/ttyACM0` or `/dev/cu.usbmodem...`.

## microSD setup

Use FAT32.

MiseDeck creates/uses:

```text
/RECEITAS/
/IMPORTAR/
/MISEDECK_BACKUP.txt
```

To import TXT recipes manually:

1. Put `.txt` recipe files inside `/IMPORTAR/`.
2. On the Cardputer, open `System > Import SD`.

## First boot

On boot, MiseDeck shows the animated intro screen.

- Press `OK` to enter immediately.
- If nothing is pressed, it enters the menu automatically after 15 seconds.

## Wi-Fi portal

On the Cardputer:

```text
Main menu > Wi-Fi > Network and Password > Connect
```

After connecting, open:

```text
http://misedeck.local
```

If mDNS does not work on your network, use the IP shown on the Cardputer.

## Troubleshooting

### Upload fails

- Confirm that the USB cable supports data.
- Close other serial monitor tools.
- Try unplugging/replugging the Cardputer.
- Manually specify the upload port.

### Portal does not open

- Confirm the Cardputer is connected to Wi-Fi.
- Use the IP address instead of `misedeck.local`.
- Make sure your phone/computer is on the same network.

### Recipes do not save

- Check if the microSD card is inserted.
- Use FAT32.
- Avoid very old or unreliable microSD cards.
