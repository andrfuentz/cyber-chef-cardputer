# v1.6.0 release checklist

## Firmware

- [x] Consolidate English and PT-BR into one firmware
- [x] Set firmware version to `v1.6.0`
- [x] Add persistent language selection
- [x] Add eight selectable color themes
- [x] Add long-name scrolling
- [x] Add double-buffered rendering
- [x] Compile the final firmware
- [x] Flash and test on a real Cardputer
- [x] Test both languages
- [x] Test color selection and persistence
- [x] Test SD recipe loading and compatibility
- [x] Test Wi-Fi portal and offline sharing
- [x] Export the final `.bin`

## Repository

- [x] Use `src/main.cpp` as the only active firmware source
- [x] Exclude personal recipe collections and local backups
- [x] Update English and Portuguese READMEs
- [x] Update feature, portal, and installation documentation
- [x] Add v1.6.0 release notes
- [ ] Update GitHub repository files
- [ ] Create tag `v1.6.0`
- [ ] Publish the GitHub release

## Public release structure

```text
README.md
README.pt-BR.md
CHANGELOG.md
LICENSE
platformio.ini
assets/
src/
docs/
media/
releases/v1.6.0/
```
