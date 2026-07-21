# Mise_Deck

**A pocket mise en place and kitchen utility firmware for the M5Stack Cardputer.**

Mise_Deck turns the M5Stack Cardputer into a small kitchen companion for recipe scaling, ingredient management, timers, offline sharing, and local browser access.

> *"we are the music makers and we are the dreamers of dreams"*

Mise_Deck was idealized by **André Fuentes** / **@anfuentz** and vibecoded with **Codex**.

## Current release

**v1.6.0** - One unified firmware with English and Portuguese/Brazilian selectable from the System menu.

## Interface preview

### Main menu

The Cardputer interface is designed around a simple amber terminal/cyberdeck style, with large menu targets and persistent navigation hints.

![Mise_Deck main menu](media/screenshots/cardputer/02_main_menu.png)

### Recipe library

Recipes are stored locally on the microSD card and organized by categories and favorites.

![Mise_Deck recipe library](media/screenshots/cardputer/03_library.png)

### Recipe overview

Recipe viewing was redesigned for the small Cardputer screen: ingredients and preps are shown one at a time with large, readable weights.

![Mise_Deck recipe overview](media/screenshots/cardputer/04_recipe_overview.png)

### Recipe actions

Each recipe has an action menu for scaling, editing ingredients, duplicating, using timers, sharing, and deleting.

![Mise_Deck recipe actions](media/screenshots/cardputer/05_recipe_actions.png)

### Timer and tools

Mise_Deck includes kitchen utilities such as timers, quick scaling, unit conversion, battery view, and sound/volume controls.

![Mise_Deck timer](media/screenshots/cardputer/06_timer.png)

### Wi-Fi and local portal

After connecting to Wi-Fi, the Cardputer serves a local browser portal at `misedeck.local` or the IP shown on screen.

![Mise_Deck Wi-Fi portal screen](media/screenshots/cardputer/07_wifi_portal.png)

## Browser portal

The browser portal is designed for phone and desktop use. It runs directly from the Cardputer on your local network; it is not a cloud service and does not require an account.

### Desktop overview

Browse recipes by category, open recipes, download TXT files, create new recipes, sync, and access backup options.

![Mise_Deck desktop browser portal](media/screenshots/portal/portal_desktop_overview.png)

### Create and edit recipes in the portal

Create and update recipes directly in the browser using clear fields for name, category, total weight, ingredients, and proportions.

![Mise_Deck browser recipe editor](media/screenshots/portal/portal_desktop_editor.png)

### Backup export

The backup menu offers full-library TXT export or individual recipe TXT files packed into a ZIP.

![Mise_Deck browser backup menu](media/screenshots/portal/portal_desktop_backup.png)

## Highlights

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
- Lets you edit, save, delete, and download recipe TXT files
- Provides TXT and ZIP backup options through the browser portal
- Shares recipes offline through a local Cardputer-hosted page
- Switches between English and Portuguese/Brazilian without changing firmware
- Includes eight selectable interface color themes
- Scrolls long recipe and ingredient names for easier reading
- Uses double-buffered rendering for smoother menus and animations

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

## Release binary

- Unified English + Portuguese/Brazilian: `releases/v1.6.0/Mise_Deck_Cardputer_v1.6.0.bin`

## Documentation

- [Install guide](docs/INSTALL.md)
- [Feature overview](docs/FEATURES.md)
- [Portal guide](docs/PORTAL.md)
- [Roadmap](docs/ROADMAP.md)
- [Release checklist](docs/RELEASE_CHECKLIST.md)
- [Portuguese README](README.pt-BR.md)

## License

Mise_Deck is released under the [MIT License](LICENSE).
