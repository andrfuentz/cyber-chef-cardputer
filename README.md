# Mise_Deck

**A pocket mise en place and kitchen utility firmware for the M5Stack Cardputer.**

Mise_Deck turns the M5Stack Cardputer into a pocket mise en place for recipe scaling, ingredient management, timers, offline sharing, and local browser access.

> *"we are the music makers and we are the dreamers of dreams"*

Mise_Deck was idealized by **André Fuentes** / **@anfuentz** and vibecoded with **Codex**.

## Status

Current public release: **v1.5.0**

English and Portuguese/Brazilian firmware builds are provided.

## Preview

### Cardputer UI

![Mise_Deck Cardputer UI preview](media/screenshots/mise_deck_v1.5_cardputer_preview.png)

### Browser portal

![Mise_Deck browser portal preview](media/screenshots/mise_deck_v1.5_portal_preview.png)

## What it does

- Stores recipes on a microSD card
- Organizes recipes by category and favorites
- Supports simple and composite recipes
- Scales recipes proportionally by total weight
- Lets you create, duplicate, edit, and delete recipes on the Cardputer
- Includes cursor editing for text and number fields
- Offers a quick scaling mode for fast ingredient calculations
- Includes timer, unit converter, battery view, and sound/volume controls
- Connects to Wi-Fi from the Cardputer
- Serves a local browser portal at `misedeck.local` or the device IP
- Provides a mobile-friendly browser interface
- Lets you edit, save, delete, and download recipe TXT files
- Provides TXT and ZIP backup options through the browser portal
- Shares recipes offline through a local Cardputer-hosted page

## Hardware

Designed for:

- M5Stack Cardputer
- M5Stack Cardputer ADV / Cardputer-Adv

The firmware uses the ESP32-S3 / StampS3 target used by the Cardputer.

## Controls

Common controls:

- `;` - up
- `.` - down
- `,` - left / previous
- `/` - right / next
- `OK` / `Enter` - confirm
- `` ` `` / `Esc` - back / cancel
- `Del` - delete character
- `Tab` - favorite/unfavorite recipe

During text or number editing:

- `,` - move cursor left
- `/` - move cursor right
- `Del` - delete before cursor

## Local browser portal

After connecting the Cardputer to Wi-Fi:

1. Open `http://misedeck.local` in a browser, or use the IP shown on the Cardputer.
2. Browse recipes by category or the All tab.
3. Open a recipe to view, scale, edit, save changes, delete, or download it.

The portal runs directly from the Cardputer on your local network. It is not a cloud service and does not require an account.

## Recipe TXT format

Mise_Deck uses a simple, human-readable TXT format:

```txt
FOCACCIA DO ANDRE
CATEGORY: DOUGHS
FAVORITE: YES
TOTAL: 700.3

[INGREDIENTS]

WATER|293.0|g
SUGAR|8.0|g
OLIVE OIL|20.0|g
WHEAT FLOUR|366.0|g
YEAST|3.3|g
SALT|10.0|g
```

Composite recipes use `[PREP]` blocks.

## Offline sharing

Mise_Deck can start an offline recipe sharing flow:

```text
Recipe > Actions > Share
```

The QR code does not require your home Wi-Fi. It helps a phone connect directly to the Cardputer sharing mode and open a local recipe page with copy/download options.

## Build

Recommended build system: PlatformIO.

```bash
pio run
pio run -t upload
```

Validated with:

- PlatformIO
- M5Cardputer `1.1.1`
- M5Unified `0.2.17`
- M5GFX `0.2.24`
- ESP32 Arduino framework

See [docs/INSTALL.md](docs/INSTALL.md) for flashing instructions.

## Release binaries

- English: `releases/v1.5.0/Mise_Deck_Cardputer_v1.5.0_EN.bin`
- Portuguese/Brazilian: `releases/v1.5.0/Mise_Deck_Cardputer_v1.5.0_PT-BR.bin`

## Documentation

- [Install guide](docs/INSTALL.md)
- [Feature overview](docs/FEATURES.md)
- [Portal guide](docs/PORTAL.md)
- [Roadmap](docs/ROADMAP.md)
- [Release checklist](docs/RELEASE_CHECKLIST.md)
- [Portuguese README](README.pt-BR.md)

## License

Mise_Deck is released under the [MIT License](LICENSE).
