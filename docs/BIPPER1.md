# BIPPER1 — Fork Gaulix Bipper L1 Pro

| | |
|:--|:--|
| **Projet** | Pager d'alerte secours — Réseau Gaulix |
| **Matériel cible** | Seeed Wio Tracker L1 Pro |
| **Environnement PlatformIO** | `seeed_wio_tracker_L1` |
| **Dépôt** | [F4EED/Bipper_L1Pro](https://github.com/F4EED/Bipper_L1Pro) |
| **Base upstream** | [meshtastic/firmware](https://github.com/meshtastic/firmware) |
| **État actuel** | **Gaulix Bipper v1.10.0** — pager secours opérationnel (alertes multi-niveaux, appartenance T1–T4, son, acquittement, ACK, historique, alarme batterie) |
| **Documents liés** | [Cahier des charges](cahier-des-charges-bip-alerte-gaulix.md) · [Roadmap phases](propositions-phases-bip-gaulix.md) · [Écosystème clients](ECOSYSTEME-GAULIX.md) |

---

## Vue d'ensemble

Ce fork transforme un **Seeed Wio Tracker L1 Pro** en terminal **pager d'alerte** pour le réseau LoRa maillé **Gaulix**, en s'appuyant sur le firmware [Meshtastic](https://meshtastic.org).

L'objectif opérationnel (secours citoyen, AASC, PCS) est décrit dans le [cahier des charges](cahier-des-charges-bip-alerte-gaulix.md). Le firmware **v1.10.0** couvre la Phase 1 et une partie de la Phase 2 : commandes `#alerte` / `#secours` / `#vigilance` / `#info` / `#fin`, filtre d'**appartenance** (`#entité` contrôlée contre T1–T4), signal pim-pom, écran plein page, acquittement, ACK DM avec horodatage et GPS, historique des alertes, carrousel UI épuré, alarme batterie faible (10 %).

Les coordinateurs envoient les commandes depuis le [client web](https://github.com/F4EED/client_web_MT_bipper) (`/alerts`) ou l'[app Android Gaulix_bipper](https://github.com/F4EED/bipper_android) — voir [Écosystème](ECOSYSTEME-GAULIX.md).

**Fonctionnalités implémentées (v1.10.0) :**

- module `GaulixPagerModule` (traitement messages, UI, buzzer, NVS) — `GAULIX_PAGER_VERSION "v1.10.0"` ;
- module `GaulixPagerAlertListModule` (historique 20 alertes, défilement Haut/Bas) ;
- commandes whitelist : `#alerte`, `#secours`, `#vigilance`, `#fin`, `#b`, `#code`, `#status`, `#Info` / `#info`, `#tag`, `#T1`…`#T4` ;
- format filaire : `#cmd <texte> [#appartenance]` — appartenance optionnelle filtrée contre les tags T1–T4 locaux ;
- son **pim-pom** (2 tons, duty 80 %) en boucle jusqu'à acquittement ; 3 séquences pour `#info` ;
- son **batterie faible** (10 %, duty 45 %, 1 bip) — répété toutes les 5 min, silencieux si charge USB ;
- écran alerte **ALERTE SECOURS** + texte + horodatage `JJ/MM HH:MM` ;
- **ACK** automatique en DM : `Pager ACK alerte JJ/MM HH:MM` (+ position GPS sur **Fr_Balise** si activée) ;
- canaux **muets** sauf **Alerte** ; région **EU868** forcée à chaque boot ;
- carrousel trackball : pages **Node**, **Bearings**, **LoRa** et favoris `*Node*` masquées ;
- 40 messages prédéfinis Gaulix (CannedMessage) ; nom usine `Bipper de demo`.

---

## Fonctionnement

### Principe général

```
  Coordinateur                    Réseau LoRa Gaulix              Bipper L1 Pro
  (app Meshtastic)         ──────────────────────────►         (seeed_wio_tracker_L1)
        ▲                                                              │
        │                                                              │
        └──────────────── Pager ACK alerte JJ/MM HH:MM ─────────────────────┘
                                      (+ GPS si activé)
```

1. Le coordinateur envoie une commande depuis l'app Meshtastic (DM ou canal **Alerte**).
2. Le pager reçoit le paquet, filtre la whitelist (`#alerte`, `#secours`, `#T1`…`#T4`, etc.).
3. L'opérateur entend le **pim-pom** en boucle, voit l'écran **ALERTE SECOURS** et acquitte (trackball).
4. Un **ACK** DM est renvoyé à l'émetteur avec horodatage et position GPS (si GPS activé).

### Comportement actuel (v1.10.0)

| Composant | Comportement |
|:----------|:-------------|
| **Compilation** | `-D GAULIX_PAGER=1` sur `seeed_wio_tracker_L1` uniquement. |
| **GaulixPagerModule** | `SinglePortModule` promiscuous ; traite les commandes avant `TextMessageModule`. |
| **Son alerte** | Séquence **pim-pom** (3100 Hz / 2400 Hz, duty 80 %, 220 ms) répétée toutes les 1,5 s jusqu'à acquittement, `#fin` ou timeout 30 min. |
| **Son info** | `#info` / `#Info` : 3 séquences pim-pom, pas d'écran alerte. |
| **Son batterie** | À **≤ 10 %** (hors charge USB) : 1 bip doux (2400 Hz, duty 45 %, 120 ms), puis rappel toutes les **5 min**. |
| **Écran alerte** | Plein page bloqué : ALERTE SECOURS, texte, `JJ/MM HH:MM`, « Appui = acquitter ». |
| **Écran accueil** | 4 lignes : nom long, `Nb AL. : N \| Der. : HH:MM`, Bipper Gaulix v1.10.0, batterie. |
| **Historique alertes** | 2ᵉ frame carrousel : 20 dernières alertes (`#alerte`, `#secours`, `#T1`–`#T4`, `#info`), scroll Haut/Bas. |
| **Carrousel UI** | Pages **Node**, **Bearings**, **LoRa** et favoris `*Node*` masquées (Gaulix uniquement). |
| **Canaux** | Tous **muets** sauf **Alerte** (réappliqué à chaque boot). |
| **Compteur alertes** | Incrémenté à chaque alerte ; **remis à zéro** à chaque allumage. |
| **Persistance** | Code, `#b`, tag service dans `/prefs/gaulixpager.cfg`. |

### Activation compile-time

Le mode pager est conditionné par le macro `GAULIX_PAGER`, défini dans la variante :

```ini
# variants/nrf52840/seeed_wio_tracker_L1/platformio.ini
build_flags = ...
  -D GAULIX_PAGER=1
```

Tout le code Gaulix est entouré de `#if defined(GAULIX_PAGER) && HAS_SCREEN` pour ne pas impacter les autres cartes.

---

## Modifications par rapport au firmware upstream

### Fichiers ajoutés

| Fichier | Rôle |
|:--------|:-----|
| `src/modules/GaulixPagerModule.h` | Déclaration du module pager (UI + compteurs statiques). |
| `src/modules/GaulixPagerModule.cpp` | Module pager : commandes, UI, son, ACK, NVS, alarme batterie. |
| `src/modules/GaulixPagerAlertListModule.h` / `.cpp` | Historique des 20 dernières alertes (2ᵉ frame UI). |
| `src/buzz/buzz.cpp` / `buzz.h` | Sons Gaulix : pim-pom alerte + bip batterie faible. |
| `src/modules/CannedMessageModule.cpp` | 40 messages prédéfinis Gaulix (usine). |
| `docs/cahier-des-charges-bip-alerte-gaulix.md` | Spécification fonctionnelle. |
| `docs/propositions-phases-bip-gaulix.md` | Roadmap par phases. |
| `docs/BIPPER1.md` | Ce document. |

### Fichiers modifiés

| Fichier | Modification |
|:--------|:-------------|
| `variants/nrf52840/seeed_wio_tracker_L1/platformio.ini` | Ajout de `-D GAULIX_PAGER=1`. |
| `src/modules/Modules.cpp` | Enregistrement `GaulixPagerModule` + `GaulixPagerAlertListModule`. |
| `src/mesh/NodeDB.cpp` | EU868 forcé ; nom long usine `Bipper de demo`. |
| `src/graphics/Screen.cpp` | Masquage carrousel (Node, Bearings, LoRa, favoris) ; blocage pendant alerte. |
| `src/graphics/draw/NodeListRenderer.cpp` | Rotation node list désactivée pour Gaulix. |

### Dépôts Git

| Remote | URL | Usage |
|:-------|:----|:------|
| **origin** | `https://github.com/F4EED/Bipper_L1Pro.git` | Fork Gaulix — dépôt de travail |
| **upstream** | `https://github.com/meshtastic/firmware.git` | Firmware Meshtastic officiel |

```bash
# Cloner le fork
git clone https://github.com/F4EED/Bipper_L1Pro.git
cd Bipper_L1Pro

# Ajouter upstream (si absent) et récupérer les mises à jour Meshtastic
git remote add upstream https://github.com/meshtastic/firmware.git
git fetch upstream
```

---

## Configuration usine Gaulix (Bipper L1 Pro)

Paramètres compilés via `userPrefs.jsonc` et appliqués au **premier boot** ou après **factory reset**. Certains réglages nécessitent encore l'application Meshtastic (voir colonne *App*).

### LoRa

| Paramètre | Valeur usine | Mécanisme |
|:----------|:-------------|:----------|
| Preset modem | `LONG_MODERATE` | `USERPREFS_LORACONFIG_MODEM_PRESET` |
| Région | EU 868 MHz | `USERPREFS_CONFIG_LORA_REGION` |
| TX activé | oui | défaut firmware (`tx_enabled=true`) |
| Puissance TX | 25 dBm | `USERPREFS_LORACONFIG_TX_POWER` |
| Slot fréquence | 1 | `USERPREFS_LORACONFIG_CHANNEL_NUM` |
| Gain RX SX126x boosté | oui | défaut firmware (`sx126x_rx_boosted_gain=true`) |
| Fréquence override | 869,4625 MHz | `USERPREFS_LORACONFIG_OVERRIDE_FREQUENCY` |
| Ignore MQTT | non | `USERPREFS_CONFIG_LORA_IGNORE_MQTT` |
| OK to MQTT | oui | `USERPREFS_CONFIG_LORA_OK_TO_MQTT` *(clé fork)* |

### Canaux (PSK `AQ==` = index court `{ 0x01 }`)

| Index | Nom | Rôle | Uplink | Downlink | Position | Précision |
|:-----:|:----|:-----|:------:|:--------:|:--------:|:---------:|
| **0** | `Fr_Balise` | PRIMARY | on | on | on | 32 (max) |
| **1** | `Fr_EMCOM` | SECONDARY | on | on | on | 32 |
| **2** | `Fr_BlaBla` | SECONDARY | on | on | on | 32 |
| **3** | `Fr_Tech` | SECONDARY | on | on | on | 32 |
| 4–6 | *(vide)* | SECONDARY | — | — | — | — |
| **7** | `Alerte` | SECONDARY | on | on | on | 32 |

> **PSK `{ 0x01 }`** : forme courte Meshtastic (clé publique par défaut). À remplacer en production.

`USERPREFS_CHANNELS_TO_WRITE: "4"` initialise les canaux **0 à 3** ; le canal **7** est initialisé séparément dans `Channels.cpp` (`case 7:` + `initDefaultChannel(7)`).

### Utilisateur

| Paramètre | Valeur usine | Mécanisme |
|:----------|:-------------|:----------|
| Nom long | `Bipper de demo` | `USERPREFS_CONFIG_OWNER_LONG_NAME` + réapplication boot si nom usine (`Meshtastic …`, `42BIP_…`) |
| Nom court | défaut Meshtastic | `%04x` du node num |
| Affichage OLED | ~18–20 caractères/ligne (128 px) | Si trop large : `Nom long : !!!trop long` |
| Stockage mesh | **24 octets UTF-8** max | `MAX_LONG_NAME_BYTES` — au-delà : tronqué par `clampLongName()` |

### Appareil

| Paramètre | Valeur usine | Mécanisme |
|:----------|:-------------|:----------|
| Rôle | `CLIENT_MUTE` | `USERPREFS_CONFIG_DEVICE_ROLE` |
| Rebroadcast | `NONE` | `USERPREFS_CONFIG_DEVICE_REBROADCAST_MODE` *(à discuter — redondant avec CLIENT_MUTE)* |
| NodeInfo broadcast | 1400 s | `USERPREFS_CONFIG_DEVICE_NODE_INFO_BROADCAST_SECS` *(clé fork)* |
| Neighbor info | désactivé | défaut firmware (`neighbor_info.enabled=false`) |

### Bluetooth

| Paramètre | Valeur usine | Mécanisme |
|:----------|:-------------|:----------|
| Activé | oui | défaut firmware |
| Mode appairage | `FIXED_PIN` | `USERPREFS_FIXED_BLUETOOTH` |
| PIN | 123456 | `USERPREFS_FIXED_BLUETOOTH` |

### Position / GPS

| Paramètre | Valeur usine | Mécanisme |
|:----------|:-------------|:----------|
| Broadcast position | 21600 s (6 h) | `USERPREFS_CONFIG_POSITION_BROADCAST_INTERVAL` |
| Smart position | activé | `USERPREFS_CONFIG_SMART_POSITION_ENABLED` |
| Distance min smart | 100 m | défaut firmware |
| Intervalle min smart | 300 s | défaut firmware |
| GPS | activé | `USERPREFS_CONFIG_GPS_MODE` |
| Intervalle MAJ GPS | 120 s | `USERPREFS_CONFIG_GPS_UPDATE_INTERVAL` |

### Messages prédéfinis (CannedMessage)

| Paramètre | Valeur usine | Mécanisme |
|:----------|:-------------|:----------|
| Messages | 40 messages Gaulix (SSL, AACS, « Reçus (Ack) », etc.) | `GAULIX_FACTORY_CANNED_MESSAGES` — réappliqués au boot |

### Réglages uniquement via l'app Meshtastic

- Chemin long complet du nom (`Dept/AASC/ODM/CRF/...`)
- Messages prédéfinis (CannedMessage)
- Fuseau horaire (`USERPREFS_TZ_STRING` est un placeholder compile-time)

### Points « à discuter »

| Sujet | Détail |
|:------|:-------|
| **Rebroadcast NONE** | `CLIENT_MUTE` inhibe déjà le rebroadcast ; le mode `NONE` est une ceinture de sécurité supplémentaire |
| **NodeInfo 1400 s** | En dessous du minimum admin (3600 s) — accepté à l'usine, l'app peut le relever si modifié |
| **PSK publique** | `{ 0x01 }` = réseau ouvert ; clés dédiées Gaulix en production |

---

## Configuration des canaux (détail technique)

Les canaux sont définis dans `userPrefs.jsonc` et compilés dans le firmware. Ils sont appliqués lors de l'**installation initiale** ou après un **factory reset** (voir section dédiée).

### Table des canaux Gaulix (résumé)

| Index | Nom | Rôle Meshtastic | PSK | Usage prévu |
|:-----:|:----|:----------------|:----|:-------------|
| **0** | `Fr_Balise` | PRIMARY | `{ 0x01 }` | Canal principal — balises / trafic mesh Gaulix |
| **1** | `Fr_EMCOM` | SECONDARY | `{ 0x01 }` | EMCOM — communications d'urgence |
| **2** | `Fr_BlaBla` | SECONDARY | `{ 0x01 }` | Trafic conversationnel |
| **3** | `Fr_Tech` | SECONDARY | `{ 0x01 }` | Canal technique |
| **7** | `Alerte` | SECONDARY | `{ 0x01 }` | Canal dédié alertes PCS / secours |

> **PSK `{ 0x01 }`** : forme courte Meshtastic (index 1 dans la table des clés par défaut). À remplacer par des clés propres au déploiement Gaulix en production.

### Mécanisme d'initialisation

`USERPREFS_CHANNELS_TO_WRITE: "4"` initialise les canaux **0, 1, 2 et 3** via une boucle dans `Channels::initDefaults()`.

Le canal **7** est initialisé séparément grâce à l'extension fork dans `Channels.cpp` :

```cpp
#ifdef USERPREFS_CHANNEL_7_NAME
    initDefaultChannel(7);
#endif
```

Le `switch` de `initDefaultChannel()` contient un `case 7:` pour appliquer nom et PSK depuis `userPrefs.jsonc`.

### Région radio

```jsonc
"USERPREFS_CONFIG_LORA_REGION": "meshtastic_Config_LoRaConfig_RegionCode_EU_868"
```

Région **EU868**, cohérente avec le réseau Gaulix et les presets Meshtastic standards.

### Factory reset — application des canaux

Les `userPrefs` ne **réécrivent pas** les canaux déjà sauvegardés en flash. Ils s'appliquent uniquement quand le fichier canaux est absent ou réinitialisé.

| Situation | Canaux Gaulix appliqués ? |
|:----------|:--------------------------|
| **Premier flash** sur appareil vierge | Oui |
| **`factory_reset_device`** (reset complet via app / admin) | Oui — efface `/prefs`, régénère la config |
| **`factory_reset_config`** (reset config partiel) | Oui — efface la config sans effacer la clé PKI |
| **Reflash firmware** sur appareil déjà configuré | Non — les canaux existants en flash sont conservés |

> **Important** : pour forcer l'application des canaux Gaulix sur un appareil déjà utilisé, effectuer un **factory reset** depuis l'application Meshtastic (*Settings → Admin → Factory reset*) **après** le flash du firmware Bipper. Le reset partiel (`factory_reset_config`) suffit si l'identité du nœud (clé PKI) doit être préservée.

---

## GaulixPagerModule et écran Etat_bipper

### Architecture

`GaulixPagerModule` hérite de `MeshModule` et s'enregistre sous le nom interne `"gaulixpager"`. Il expose une frame UI via `wantUIFrame() → true`, intégrée automatiquement dans la rotation d'écrans Meshtastic (`Screen.cpp` → `GetMeshModulesWithUIFrames`).

L'écran fonctionnel est désigné **Etat_bipper** : c'est l'écran d'accueil pager montrant l'état d'écoute du terminal.

### Disposition à l'écran (OLED 128×64, `FONT_SMALL`)

```
┌─────────────────────────────┐
│ Bipper de demo              │  ← L1 : nom long (ou « En écoute »)
│ Nb AL. : 0 | Der. : --:--   │  ← L2 : compteur | dernière heure
│   Bipper Gaulix v1.10.0     │  ← L3 : titre + version (centré)
│ Batterie : 78 %             │  ← L4 : niveau batterie
│ Trackball >                 │  ← hint historique alertes (frame 2)
└─────────────────────────────┘
```

| Zone | Détail |
|:-----|:-------|
| Ligne 1 | `owner.long_name` ; si absent → `En écoute` ; si trop large pour l'OLED → `Nom long : !!!trop long` |
| Ligne 2 | `Nb AL. : N \| Der. : HH:MM` (ou `--:--`) |
| Ligne 3 | `Bipper Gaulix v1.10.0` (`GAULIX_PAGER_TITLE`) |
| Ligne 4 | Batterie %, `USB`, ou `--` |
| Compteur | Remis à **zéro** à chaque allumage ; `#info` n'incrémente pas |
| Frame 2 | Historique alertes (`GaulixPagerAlertListModule`) — scroll Haut/Bas |

### Écran alerte (plein page)

```
┌─────────────────────────────┐
│      ALERTE SECOURS         │
│  <texte du message>         │
│       13/07 16:45           │
│    Appui = acquitter        │
└─────────────────────────────┘
```

### API publique

```cpp
static uint32_t getAlertCount();
static uint32_t getLastAlertTime();
static bool isAlertActive();
void userAcknowledgeAlert();
```

`recordAlert()` est appelé par `triggerAlert()` à chaque alerte `#alerte` / `#secours` / `#T1`…`#T4`.

### Navigation

L'écran **Etat_bipper** et l'**historique alertes** apparaissent dans le carrousel Meshtastic (trackball haut/bas). Les pages standard **Node**, **Bearings**, **LoRa** et les favoris `*Node*` sont masquées sur le build Gaulix. Le carrousel automatique est désactivé.

---

## Matériel — Seeed Wio Tracker L1 Pro

| Ressource | Détail | Usage pager |
|:----------|:-------|:------------|
| MCU | nRF52840 | Exécution firmware, bootloader UF2 |
| Radio | SX1262 LoRa | Réseau Gaulix EU868 |
| Écran | OLED SSD1306 128×64 | Accueil pager + alertes plein écran |
| Buzzer | D12 (`PIN_BUZZER`) | Alerte : pim-pom PWM 80 %, 3100/2400 Hz ; batterie 10 % : 1 bip 45 % |
| LED alerte | **PIN_LED1** | Clignotement pendant alerte *(LED2 partageait le buzzer — corrigé)* |
| Trackball + bouton | TB_* / D13 | Acquittement alerte |
| GPS L76K | UART | Position dans l'ACK DM si `gps_mode = ENABLED` |

La variante déclare `custom_meshtastic_requires_dfu = true` : le flashage passe par le bootloader **UF2** nRF52.

---

## Compilation et flashage

### Prérequis

- [PlatformIO](https://platformio.org/) (CLI ou extension VS Code / Cursor)
- Câble USB vers le L1 Pro
- Sous Windows : pilote USB série installé ; le volume UF2 apparaît en mode bootloader

### Compilation

```bash
cd firmware_meshtastic
pio run -e seeed_wio_tracker_L1
```

| Sortie | Chemin |
|:-------|:-------|
| Binaire UF2 | `C:\pio-build\seeed_wio_tracker_L1\firmware-seeed_wio_tracker_L1-2.8.0.*.uf2` |
| Hex (intermédiaire) | `.pio/build/seeed_wio_tracker_L1/firmware.hex` |

Le script `extra_scripts/nrf52_extra.py` génère automatiquement le `.uf2` depuis le `.hex` via `bin/uf2conv.py`.

### Flashage UF2

1. **Entrer en mode bootloader** sur le L1 Pro :
   - double-appui rapide sur le bouton **RESET**, ou
   - touch 1200 baud sur le port série (`touch_1200bps` via meshtastic-mcp).
2. Un lecteur USB nommé type **WIO Tracker** ou **XIAO** apparaît.
3. **Copier** `firmware.uf2` à la racine de ce lecteur.
4. L'appareil redémarre automatiquement avec le nouveau firmware.

### Flashage via PlatformIO (alternative)

```bash
pio run -e seeed_wio_tracker_L1 -t upload --upload-port COMx
```

Remplacer `COMx` par le port série détecté (ex. `COM7` sous Windows).

### Après le premier flash

1. Allumer le bippeur et le configurer dans l'app Meshtastic (nom du nœud, etc.).
2. Si l'appareil avait une config antérieure : effectuer un **factory reset** pour appliquer les canaux Gaulix.
3. Vérifier dans l'app que les canaux `Fr_Balise`, `Fr_EMCOM`, `Fr_BlaBla`, `Fr_Tech` et `Alerte` sont présents.
4. L'écran d'accueil **Bipper Gaulix v1.10.0** s'affiche (frame pager dédiée).

---

## Commandes implémentées (v1.10.0)

Format général des alertes :

```text
#alerte|#secours|#vigilance|#info <texte libre> [#appartenance]
#fin [#appartenance]
```

| Commande | Action | Exemple |
|:---------|:-------|:--------|
| `#alerte <texte> [#app]` | Alerte secours | `#alerte Rassemblement hall` · `#alerte Feu #odm42` |
| `#secours <texte> [#app]` | Synonyme / variante alerte | `#secours Renforts Nord` |
| `#vigilance <texte> [#app]` | Niveau vigilance | `#vigilance Crue attendue #aasc` |
| `#info <texte> [#app]` | 3× pim-pom, pas d'écran alerte plein page | `#info Exercice terminé` |
| `#fin [#app]` | Fin d'alerte à distance (2 bips fin) | `#fin` · `#fin #odm42` |
| `#T1`…`#T4 <texte>` | Alerte legacy si tag local correspond | `#T1 Intervention` |
| `#tag …` | Définir les valeurs T1–T4 persistantes | via client web / app Android |
| `#b <n>` | Nombre de bips config (NVS) | `#b 3` |
| `#code <ancien> <nouveau>` | Code d'activation persistant | `#code GAULIX GAULIX26` |
| `#status` | État du pager en DM | `#status` |

**Appartenance :** seul le **dernier** jeton `#…` non réservé est l'appartenance. À la réception, le Bipper compare ce jeton à ses tags **T1–T4** ; s'il ne correspond à aucun, l'alerte est ignorée. Sans `#appartenance`, tous les Bippers concernés réagissent.

> **Whitelist stricte** : tout autre texte (ex. `test`) est ignoré par `GaulixPagerModule` (pas d'alerte, pas de buzzer).

### Son, acquittement et sécurités

| Événement | Son | Écran | ACK DM |
|:----------|:----|:------|:-------|
| `#alerte` / `#secours` / `#vigilance` / `#T1`…`#T4` | Pim-pom en boucle (1,5 s) | Plein page jusqu'à acquittement | Non |
| Acquittement (trackball) | 1 bip fin | Retour accueil immédiat | `Pager ACK alerte JJ/MM HH:MM` (+ GPS Fr_Balise) |
| `#fin` | 2 bips fin | Retour accueil | Non |
| `#info` | 3× pim-pom puis silence | Inchangé | Non |
| Batterie ≤ 10 % (hors USB) | 1 bip doux, rappel 5 min | Inchangé | Non |
| 30 min sans acquittement | Arrêt silencieux | Retour accueil | Non |

---

## Fonctionnalités — état d'avancement

Roadmap : [propositions-phases-bip-gaulix.md](propositions-phases-bip-gaulix.md).

### Phase 1 — Pager secours *(implémentée — v1.10.0)*

| Fonction | Statut | Description |
|:---------|:------:|:------------|
| Écran d'accueil pager | ✅ | 4 lignes : nom, `Nb AL.` / `Der.`, version, batterie |
| Historique alertes (20 entrées) | ✅ | `GaulixPagerAlertListModule`, scroll Haut/Bas |
| Carrousel UI épuré | ✅ | Node, Bearings, LoRa, favoris masqués |
| Alarme batterie 10 % | ✅ | Bip doux (45 % duty), rappel 5 min, muet si USB |
| Canaux Gaulix + muet sauf Alerte | ✅ | ch0–3 + ch7 ; mute réappliqué au boot |
| EU868 forcé au boot | ✅ | `NodeDB` + `GaulixPagerModule` |
| `#alerte` / `#secours` / `#vigilance` | ✅ | Niveaux d'alerte + pim-pom |
| Appartenance `#entité` vs T1–T4 | ✅ | Filtre à la réception (v1.10) |
| `#T1`…`#T4` + `#tag` | ✅ | Alertes / tags de service |
| `#Info` / `#info` | ✅ | 3× pim-pom, pas d'écran alerte |
| `#fin` | ✅ | Fin d'alerte à distance (+ appartenance optionnelle) |
| Son pim-pom continu | ✅ | Jusqu'à acquittement, `#fin` ou 30 min |
| Écran plein page alerte | ✅ | ALERTE SECOURS + texte + horodatage |
| LED clignotante | ✅ | PIN_LED1 pendant alerte |
| Acquittement trackball | ✅ | ACK DM + bip fin |
| ACK automatique + GPS | ✅ | `Pager ACK alerte JJ/MM HH:MM \| lat lon` |
| Code d'activation `#code` | ✅ | Persistance `gaulixpager.cfg` |
| `#status` | ✅ | État pager en DM |
| Timeout 30 min | ✅ | Coupure auto sans acquittement |
| Messages prédéfinis Gaulix | ✅ | 40 messages CannedMessage |
| Clients coordinateurs | ✅ | Web `/alerts` + Android Gaulix_bipper |
| `#b <n>` | ⚠️ | Enregistré en NVS |
| Code obligatoire dans `#alerte` | ⏳ | Non requis (whitelist syntaxe) |
| Fiche réflexe opérateur | ⏳ | PDF 1 page |

### Phase 2 — Niveaux d'alerte *(partiellement couvert en v1.10)*

- `#info` / `#vigilance` / `#alerte` / `#secours` + appartenance : ✅
- `#urgence` et niveaux visuels distincts sur OLED : à venir
- Liste blanche des coordinateurs
- Mode exercice vs réel

### Phase 3 — Réseau de crise *(V2)*

- Carte des volontaires (GPS)
- Statuts bénévoles (« En route », « Sur place »…)
- Store & Forward pour alertes différées
- Pont MQTT vers supervision
- Port `ALERT_APP` Meshtastic

### Phase 4 — Interface mode crise *(V3)*

- Navigation trackball enrichie (historique, statuts, messages PCS)
- Terminal de coordination léger

---

## Tests et validation

### Vérifications manuelles (v1.10.0)

| Test | Résultat attendu |
|:-----|:-----------------|
| Build `seeed_wio_tracker_L1` | Compilation OK, UF2 dans `C:\pio-build\` |
| Boot | Écran accueil : nom, `Nb AL. : 0`, `Bipper Gaulix v1.10.0` |
| Carrousel | Pas de pages Node, Bearings, LoRa, `*Node*` |
| Batterie ≤ 10 % | 1 bip doux, rappel 5 min (silencieux si USB) |
| `#info test` | 3× pim-pom, pas d'écran alerte |
| `#alerte essai` | Pim-pom continu + écran ALERTE SECOURS bloqué |
| Acquittement | Silence + ACK DM + position Fr_Balise |
| `#fin` | Arrêt alerte + 2 bips fin, sans ACK |
| Canaux | Seul **Alerte** non muet |

### Tests automatisés upstream

```bash
./bin/run-tests.sh
```

Les tests natifs Meshtastic ne couvrent pas encore `GaulixPagerModule`. Les tests matériel passent par [meshtastic-mcp](https://github.com/meshtastic/meshtastic-mcp) avec `MESHTASTIC_FIRMWARE_ROOT` pointant vers ce dépôt.

---

## Structure des fichiers clés

```
firmware_meshtastic/
├── docs/
│   ├── BIPPER1.md                          ← ce document
│   ├── cahier-des-charges-bip-alerte-gaulix.md
│   └── propositions-phases-bip-gaulix.md
├── src/
│   ├── modules/
│   │   ├── GaulixPagerModule.h
│   │   ├── GaulixPagerModule.cpp
│   │   └── Modules.cpp                     ← instanciation conditionnelle
│   └── mesh/
│       └── Channels.cpp                    ← init canal 7
├── variants/nrf52840/seeed_wio_tracker_L1/
│   ├── platformio.ini                      ← GAULIX_PAGER=1
│   └── variant.h                           ← broches L1 Pro
└── userPrefs.jsonc                         ← canaux et région Gaulix
```

---

## Historique document

| Version | Date | Auteur | Notes |
|:--------|:-----|:-------|:------|
| 1.4 | 20/07/2026 | Réseau Gaulix | Alignement **v1.10.0** : `#vigilance`, appartenance `#entité` vs T1–T4, liens clients web/Android |
| 1.3 | 13/07/2026 | Réseau Gaulix | v1.6 : alarme batterie 10 %, historique alertes, carrousel épuré, `Nb AL.` / `Der.` |
| 1.2 | 13/07/2026 | Réseau Gaulix | v1.4 : alertes, pim-pom, ACK GPS, écran 4 lignes, tags T1–T4 |
| 1.1 | 06/07/2026 | Réseau Gaulix | Configuration usine complète (LoRa, canaux, rôle, BT, GPS) |
| 1.0 | 06/07/2026 | Réseau Gaulix | Documentation initiale de l'état fork Bipper1 |

---

*Fork maintenu par [F4EED/Bipper_L1Pro](https://github.com/F4EED/Bipper_L1Pro) — base [meshtastic/firmware](https://github.com/meshtastic/firmware).*
