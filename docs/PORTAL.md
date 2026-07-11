# Local web portal

Cyber Chef includes a local web portal served directly by the Cardputer.

## Access

Connect the Cardputer to Wi-Fi, then open:

```text
http://cyberchef.local
```

If that does not work, use the IP address displayed on the Cardputer.

## Main sections

- All recipes
- Favorites
- Category tabs
- New simple recipe
- New composite recipe
- Sync
- Backup download

## Mobile layout

The portal is designed to work on phones:

- Large touch targets
- Horizontal category tabs
- Full-screen recipe modal
- Bigger recipe text
- Larger editor area
- Mobile-friendly scaling controls

## Recipe editor

Cyber Chef uses a simple TXT format.

Example:

```txt
FOCACCIA DO ANDRE
CATEGORY: PASTA
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

## Sync button

Use `SYNC` after:

- editing recipes on the Cardputer;
- saving through the browser;
- creating a new recipe;
- importing from SD.

## Limitations

- The portal works only on the same local network.
- It is not an internet cloud service.
- `cyberchef.local` depends on mDNS support; IP address is more reliable.
