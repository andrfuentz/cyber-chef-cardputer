# v1.0.0 release checklist

## Firmware

- [x] Decide initial v1.0.0 firmware language: English first, PT-BR build/language selector planned next
- [x] Change `FW_VERSION` to `v1.0.0`
- [x] Compile final firmware
- [ ] Flash and test on real Cardputer
- [x] Test SD recipe save/load
- [ ] Test Wi-Fi connection
- [ ] Test browser portal on desktop
- [x] Test browser portal on phone
- [x] Test QR offline sharing
- [ ] Test Portal cake easter egg
- [x] Export final `.bin`

## Repository

- [x] Choose license: MIT
- [x] Add final `LICENSE`
- [x] Review README English
- [x] Review README Portuguese
- [ ] Add screenshots
- [ ] Add photos
- [ ] Add demo video link
- [ ] Remove old/private/local artifacts if needed
- [x] Add release notes

## Suggested repository cleanup

Before publishing, keep only the clean project files and release assets.

Recommended public structure:

```text
README.md
README.pt-BR.md
CHANGELOG.md
LICENSE
platformio.ini
MiseDeck_Cardputer_v11.ino
chef_splash.h
assets/
src/
docs/
media/
releases/
```

Avoid publishing:

- old temporary zips;
- local build cache;
- `.pio/`;
- random work/prototype folders;
- serial logs;
- personal Wi-Fi credentials.

## License decision

MiseDeck uses the MIT License.
