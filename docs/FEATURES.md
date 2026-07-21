# Feature overview

Mise_Deck is designed as a compact kitchen utility firmware for the M5Stack Cardputer.

## Language and appearance

- One firmware with English and Portuguese/Brazilian
- Language preference saved between restarts
- Eight selectable terminal-style color themes
- Smooth double-buffered screen rendering
- Automatic scrolling for long recipe, preparation, and ingredient names

## Recipe library

- Store recipes on microSD
- Browse by category
- Mark recipes as favorites
- Create simple recipes
- Create composite recipes with preparations
- Duplicate recipes
- Delete recipes with confirmation

## Scaling

Mise_Deck is built around weight-based recipe scaling.

You can open a recipe and change the total target weight. Ingredient weights are recalculated proportionally.

## Ingredient editing

Supported actions:

- Add ingredient
- Rename ingredient
- Change ingredient weight
- Remove ingredient

Text and number fields include cursor movement for correcting entries on the Cardputer keyboard.

## Quick mode

Quick mode is for fast proportional calculations without saving a full recipe.

Example use:

1. Enter several ingredient weights.
2. Enter a new target total.
3. Mise_Deck recalculates the new weights.

## Timer

Timer presets:

- Custom
- 1 minute
- 3 minutes
- 5 minutes
- 10 minutes
- 15 minutes

The timer includes a sound alert when finished.

## Sound

System sound options:

- On/off
- Volume control
- Navigation beep
- Confirmation/error sounds
- Timer melody
- Recipe save jingle

## Battery

Mise_Deck shows a simple battery percentage indicator on screen and includes a battery screen with:

- Percentage
- Voltage
- Bar indicator

## Wi-Fi portal

When connected to Wi-Fi, Mise_Deck starts a local portal:

```text
misedeck.local
```

or by IP address.

Portal features:

- Browse recipes by category
- Mobile-friendly layout
- Open recipe details
- Recalculate proportions
- Edit TXT
- Save changes
- Create new simple/composite recipe
- Download TXT
- Download backup
- Download all recipes as individual TXT files in a ZIP archive

## Offline sharing

Recipes can be shared without using your home Wi-Fi:

```text
Recipe > Actions > Share
```

The Cardputer starts a local sharing flow so a phone can open a recipe page with copy/download options.


