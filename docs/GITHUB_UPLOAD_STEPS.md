# GitHub upload steps

This repository is prepared as a simple manual upload package.

## Recommended repository settings

- Repository name: `cyber-chef-cardputer`
- Visibility: Public
- License: MIT
- Main language: C++
- Short description: `Cyberdeck-style kitchen utility firmware for the M5Stack Cardputer`
- Website: leave blank unless you later add a project page

## Manual upload through the GitHub website

1. Create a GitHub account or log in at `github.com`.
2. Click `+` in the top-right corner.
3. Choose `New repository`.
4. Use the repository name `cyber-chef-cardputer`.
5. Choose `Public`.
6. Do not add a README, `.gitignore`, or license on GitHub, because they are already included here.
7. Click `Create repository`.
8. On the next screen, choose `uploading an existing file`.
9. Open the prepared folder `CyberChef_GitHub_Release_v1.0.0`.
10. Drag all files and folders from inside that folder into GitHub.
11. Wait for the upload to finish.
12. Use the commit message `Initial public release`.
13. Click `Commit changes`.

## Release upload

After the files are uploaded:

1. Open the repository page.
2. Click `Releases` on the right side, or open the `Tags/Releases` area.
3. Click `Create a new release`.
4. Tag: `v1.0.0`.
5. Release title: `Cyber Chef v1.0.0 EN / v1.1 PT-BR`.
6. Copy the text from `docs/RELEASE_NOTES_v1.0.0.md`.
7. Attach the binary files from:
   - `releases/v1.0.0/CyberChef_Cardputer_v1.0.0_EN.bin`
   - `releases/v1.1.0-PTBR/CyberChef_Cardputer_v1.1_PT-BR.bin`
   - `releases/CyberChef_Public_Release_EN_PTBR.zip`
8. Click `Publish release`.

## After publishing

Add real photos, screenshots, and a short demo video when ready:

- `media/photos/`
- `media/screenshots/`
- `media/video/`
