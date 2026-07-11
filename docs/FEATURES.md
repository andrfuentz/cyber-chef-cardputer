# Feature overview

Cyber Chef is designed as a compact kitchen utility firmware for the M5Stack Cardputer.

## Recipe library

- Store recipes on microSD
- Browse by category
- Mark recipes as favorites
- Create simple recipes
- Create composite recipes with preparations
- Duplicate recipes
- Delete recipes with confirmation

## Scaling

Cyber Chef is built around weight-based recipe scaling.

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
3. Cyber Chef recalculates the new weights.

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

Cyber Chef shows a simple battery percentage indicator on screen and includes a battery screen with:

- Percentage
- Voltage
- Bar indicator

## Wi-Fi portal

When connected to Wi-Fi, Cyber Chef starts a local portal:

```text
cyberchef.local
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

## Offline QR sharing

Recipes can be shared without Wi-Fi using a QR Code summary:

```text
Recipe > Actions > Share
```

The QR contains a compact text summary:

```text
RECIPE NAME
TOTAL:1300.0g
INGREDIENT 1 100.0g-INGREDIENT 2 200.0g
```

## About screen and easter egg

The About screen credits:

- André Fuentes / @anfuentz — concept and project direction
- Codex — vibecoding collaborator

The hidden sequence unlocks a translated and metric-converted Portal cake recipe.
