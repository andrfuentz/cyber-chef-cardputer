# MiseDeck v1.0.0 EN / v1.1 PT-BR

First public release candidate of MiseDeck for the M5Stack Cardputer.

MiseDeck is a cyberdeck-style kitchen utility firmware focused on recipe scaling, ingredient management, local-first storage, and practical kitchen tools on a tiny device.

## Highlights

- Recipe library stored on microSD
- Simple and composite recipes
- Categories and favorites
- Proportional recipe scaling by total weight
- Ingredient creation and editing directly on the Cardputer
- Quick scaling mode
- Timer and unit converter
- Battery indicator and battery info screen
- Sound menu with volume control
- Wi-Fi setup from the device
- Local browser portal at `misedeck.local` or by IP
- Mobile-friendly portal layout
- TXT recipe editing and download
- Offline QR recipe sharing
- About screen with a hidden Portal cake recipe easter egg

## Language note

The main public source build is English (`v1.0.0 EN`) for easier international sharing.

A rebuilt Portuguese/Brazilian firmware build is also included as `v1.1 PT-BR`.

## Audio note

The opening screen includes a short celesta/music-box style generated-frequency motif. No audio samples, MP3, or WAV files are used.

## Included example recipe

The clean first-boot recipe library includes:

- `FOCACCIA DO ANDRE`

## Flashing

Use the included `.bin` file with your preferred ESP32 flashing workflow, or build from source with PlatformIO:

```bash
pio run
pio run -t upload
```

See `docs/INSTALL.md` for more details.

## Credits

MiseDeck was idealized by André Fuentes / @anfuentz and vibecoded with Codex.
