# GitHub update steps for v1.6.0

Repository: `andrfuentz/misedeck-cardputer`

## Update the repository

1. Upload the updated project files while keeping their folder structure.
2. Remove the obsolete `CyberChef_Cardputer_v11.ino` file if it still exists on GitHub.
3. Remove the obsolete `variants/` folder if it still exists on GitHub.
4. Remove any personal recipe folders or conversion reports if they exist on GitHub.
5. Confirm that `src/main.cpp` is the only active firmware source.
6. Use the commit message `Prepare Mise_Deck v1.6.0`.

## Publish the release

1. Open the repository's **Releases** page.
2. Choose **Draft a new release**.
3. Create the tag `v1.6.0` from the main branch.
4. Use the title `Mise_Deck v1.6.0`.
5. Copy the text from `docs/RELEASE_NOTES_v1.6.0.md`.
6. Attach:
   - `releases/v1.6.0/Mise_Deck_Cardputer_v1.6.0.bin`
   - `releases/v1.6.0/SHA256SUMS.txt`
7. Confirm that the release is not marked as a pre-release.
8. Publish the release.

## Repository settings

- Visibility: Public
- License: MIT
- Main language: C++
- Description: `Cyberdeck-style kitchen utility firmware for the M5Stack Cardputer`
