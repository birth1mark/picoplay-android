# PicoPlay Android — 8-Bit Fantasy Console Player & Pocket Arcade

<div align="center">

![Platform](https://img.shields.io/badge/Platform-Android%207.0%2B%20(API%2024--35)-3DDC84?style=for-the-badge&logo=android&logoColor=white)
![Architecture](https://img.shields.io/badge/ABI-arm64--v8a%20%7C%20x86__64-00979D?style=for-the-badge)
![PageSize](https://img.shields.io/badge/Memory%20Page%20Size-16KB%20Aligned-brightgreen?style=for-the-badge)
![License](https://img.shields.io/badge/License-MIT%20%2B%20Zlib-yellow?style=for-the-badge)

**PicoPlay Android** brings the 128×128 8-bit fantasy console experience to smartphones, tablets, Android handhelds (Retroid Pocket, Odin, Anbernic), and Android TV with a custom **Pocket Touch Gamepad**, Bluetooth/USB controller auto-detection, local `.p8`/`.p8.png` cartridge importing, and an optional in-app BBS Store.

> **Companion PlayStation 4 Edition:** Check out [**PicoPlay PS4 (`birth1mark/picoplay-ps4`)**](https://github.com/birth1mark/picoplay-ps4) for the native OpenOrbis PlayStation 4 Fake PKG build.

</div>

---

## Screenshots

<p align="center">
  <img src="docs/screenshots/pocket-arcade.png" width="30%" alt="PicoPlay Pocket Arcade Menu" />
  <img src="docs/screenshots/pocket-bbs.png" width="30%" alt="PicoPlay BBS Store" />
  <img src="docs/screenshots/pocket-keypad.png" width="30%" alt="PicoPlay BBS ID Keypad" />
</p>

---

## Two Editions (Product Flavors)

| Edition | Application ID | Target | Features |
| :--- | :--- | :--- | :--- |
| **PicoPlay Full (`full`)** | `com.birth1mark.picoplay.full` | Direct APK / GitHub Releases / Handhelds | Pre-loaded starter cartridges + **In-App BBS Store** + Local `.p8`/`.p8.png` file importer |
| **PicoPlay Play Store (`playstore`)** | `com.birth1mark.picoplay` | Google Play Store (`.aab` & `.apk`) | 100% Google Play policy-compliant, Target SDK 35 (Android 15), 16KB page size aligned, open-source starter cartridges + Local `.p8`/`.p8.png` file importer via Android Storage Access Framework |

---

## Controls

### Pocket Touch Gamepad (Smartphones & Tablets)
- **D-Pad (Bottom Left)**: 8-way directional input (`Up`, `Down`, `Left`, `Right` + diagonals).
- **`O` Button (Red, Bottom Right)**: Primary / Jump / Confirm (`Z` / `C`).
- **`X` Button (Blue, Bottom Right)**: Secondary / Dash / Action (`X` / `V`).
- **`[MENU]` (Top Left)**: Opens the in-game Pause / Settings / Reset menu.
- **`[+CART]` (Top Center)**: Opens the native Android file picker to import any `.p8` or `.p8.png` cartridge from your Downloads folder or cloud storage.
- **`[SCALE]` (Top Right)**: Cycles between Pixel-Perfect Integer Scaling and Fit-to-Bezel mode.

### Physical Bluetooth / USB Controllers (DualShock 4, DualSense, Xbox, 8BitDo, Retroid/Odin)
- **Auto-Hide Overlay**: Pressing any button on a physical controller automatically hides the on-screen touch gamepad for an unobstructed display. Touching the screen brings the overlay back immediately.

---

## Building from Source

```bash
cd android
./gradlew assembleFullRelease assemblePlaystoreRelease bundlePlaystoreRelease
```

### Output Artifacts
- **Full APK (with BBS Store)**: `android/app/build/outputs/apk/full/release/app-full-release.apk`
- **Play Store APK**: `android/app/build/outputs/apk/playstore/release/app-playstore-release.apk`
- **Google Play App Bundle (`.aab`)**: `android/app/build/outputs/bundle/playstoreRelease/app-playstore-release.aab`

---

## Legal & Trademark Notice

**PicoPlay** is an independent open-source fantasy console interpreter based on Fake-08 and SDL2. **PicoPlay is not affiliated with, endorsed by, or sponsored by Lexaloffle Games LLP.** "PICO-8" is a registered trademark of Lexaloffle Games LLP. See [LEGAL.md](LEGAL.md) and [PRIVACY_POLICY.md](PRIVACY_POLICY.md) for details.
