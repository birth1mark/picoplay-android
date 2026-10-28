# Google Play Console Store Listing Guide — PicoPlay

This document contains all ready-to-paste fields, assets, and compliance questionnaire answers required to publish **PicoPlay** (`com.birth1mark.picoplay`) on Google Play Console.

---

## 1. App Details (English — `en-US`)

- **App Name (max 30 chars):**
  `PicoPlay — 8-Bit Fantasy Arcade` *(30 characters)*
- **Short Description (max 80 chars):**
  `Play 128x128 8-bit fantasy console cartridges with touch & gamepad support.` *(76 characters)*
- **Full Description (max 4000 chars):**
  ```text
  PicoPlay is a fast, native 8-bit fantasy console player and retro pocket arcade designed for Android phones, tablets, handheld gaming consoles, and Android TV.

  Experience classic 128x128 pixel-art games with a vibrant 16-color palette, 4-channel chiptune audio synthesizer, and zero-latency controls.

  KEY FEATURES:
  • Built-In Starter Arcade Library: Jump straight into pre-installed open-source 8-bit classics right out of the box.
  • Import Any .P8 or .P8.PNG Cartridge: Tap [+CART] or [OPEN] to load your own ".p8" source cartridges or ".p8.png" image cartridges directly from your device storage.
  • Pocket Touch Gamepad: Responsive multi-touch Virtual Thumb Joystick and O/X arcade buttons designed for both Portrait and Landscape play.
  • Automatic Bluetooth & USB Controller Support: Connect any Xbox, PlayStation DualShock/DualSense, 8BitDo, or built-in handheld controller—the on-screen touch overlay hides automatically when a physical controller is pressed.
  • Authentic 60 FPS Native C++ Engine: Powered by SDL2 and OpenGL ES 2.0 with integer pixel-perfect scaling and 16KB memory page alignment for Android 15+.
  • 100% Offline & Private: No ads, no tracking, no analytics, and no internet connection required.

  LEGAL DISCLAIMER:
  PicoPlay is an independent open-source interpreter built on Fake-08. PicoPlay is not affiliated with, endorsed by, or sponsored by Lexaloffle Games LLP.
  ```

---

## 2. Detalhes da Aplicação (Português — `pt-PT`)

- **Nome da aplicação (máx. 30 carateres):**
  `PicoPlay — Arcade Retro 8-Bit` *(29 carateres)*
- **Descrição breve (máx. 80 carateres):**
  `Corre cartuchos 8-bit 128x128 com comando tátil de bolso e Bluetooth.` *(70 carateres)*
- **Descrição completa (máx. 4000 carateres):**
  ```text
  PicoPlay é um leitor nativo de cartuchos de consola de fantasia 8-bit e arcade de bolso desenhado para smartphones Android, tablets, consolas portáteis e Android TV.

  Desfruta de jogos retro pixel-art a 128x128 com paleta de 16 cores, sintetizador chiptune de 4 canais e controlos de baixa latência a 60 FPS.

  FUNCIONALIDADES PRINCIPAIS:
  • Biblioteca Inicial Incluída: Começa a jogar de imediato com os cartuchos clássicos open-source pré-instalados.
  • Importador de Cartuchos (.P8 e .P8.PNG): Toca em [+CART] para importar qualquer cartucho a partir da memória do teu dispositivo.
  • Comando Tátil Pocket Gamepad: Virtual Thumb Joystick analógico multi-toque e botões O/X otimizados para modo Vertical (Portrait) e Horizontal (Landscape).
  • Suporte Automático para Comandos Bluetooth/USB: Ao premires qualquer botão num comando físico, os botões táteis no ecrã ocultam-se automaticamente.
  • 100% Privado e Sem Publicidade: Sem anúncios, sem rastreadores e totalmente funcional offline.

  AVISO LEGAL:
  PicoPlay é um projeto open-source independente baseado no motor Fake-08 e não é afiliado nem patrocinado pela Lexaloffle Games LLP.
  ```

---

## 3. Graphic Assets (Included in Repository)

- **App Icon (512×512 PNG):** `docs/store_assets/playstore-icon-512.png`
- **Feature Graphic (1024×500 PNG):** `docs/store_assets/feature-graphic-1024x500.png`
- **Phone Screenshots (1080×1920 PNG):**
  - `docs/screenshots/pocket-arcade.png`
  - `docs/screenshots/pocket-bbs.png`
  - `docs/screenshots/pocket-keypad.png`

---

## 4. Google Play Console Compliance Answers

- **Privacy Policy URL:**
  `https://github.com/birth1mark/picoplay-android/blob/main/PRIVACY_POLICY.md`
- **App Category:** Game -> Arcade
- **Data Safety Questionnaire:**
  - *Does your app collect or share any of the required user data types?* -> **No**
- **IARC Content Rating Questionnaire:**
  - Category: Game
  - User-generated online interaction / chat: **No**
  - Mild fantasy pixel violence (e.g., retro spaceship asteroids / platforming): **Yes (Mild Fantasy Violence)** -> Results in **PEGI 3 / Everyone (E)**.
- **App Bundle to Upload:**
  `android/app/build/outputs/bundle/playstoreRelease/app-playstore-release.aab`
