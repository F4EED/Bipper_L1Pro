# BIPPER1 — Fork Gaulix Bipper L1 Pro

| | |
|:--|:--|
| **Projet** | Pager d'alerte secours — Réseau Gaulix |
| **Matériel cible** | Seeed Wio Tracker L1 Pro |
| **Environnement PlatformIO** | `seeed_wio_tracker_L1` |
| **Dépôt** | [F4EED/Bipper_L1Pro](https://github.com/F4EED/Bipper_L1Pro) |
| **Base upstream** | [meshtastic/firmware](https://github.com/meshtastic/firmware) |
| **État actuel** | Phase 1 amorcée — écran d'accueil pager + canaux Gaulix par défaut |
| **Documents liés** | [Cahier des charges](cahier-des-charges-bip-alerte-gaulix.md) · [Roadmap phases](propositions-phases-bip-gaulix.md) |

---

## Vue d'ensemble

Ce fork transforme un **Seeed Wio Tracker L1 Pro** en terminal **pager d'alerte** pour le réseau LoRa maillé **Gaulix**, en s'appuyant sur le firmware [Meshtastic](https://meshtastic.org).

L'objectif opérationnel (secours citoyen, AASC, PCS) est décrit dans le cahier des charges : à terme, réception d'une commande `#alerte`, signal sonore et visuel, affichage plein écran, acquittement local et accusé de réception au coordinateur.

**À la date de ce document**, le firmware intègre déjà :

- l'activation compile-time du mode pager (`GAULIX_PAGER`) sur la variante L1 ;
- un module UI `GaulixPagerModule` affichant l'écran d'état du bippeur ;
- la préconfiguration des canaux réseau Gaulix via `userPrefs.jsonc` ;
- une extension de `Channels.cpp` pour initialiser le canal **Alertes** (index 7).

Les fonctions d'alerte complètes (`#alerte`, buzzer, acquittement, ACK) sont **planifiées** mais **pas encore implémentées** dans le code.

---

## Fonctionnement

### Principe général

```
  Coordinateur                    Réseau LoRa Gaulix              Bipper L1 Pro
  (app Meshtastic)         ──────────────────────────►         (seeed_wio_tracker_L1)
        ▲                                                              │
        │                                                              │
        └──────────────── ACK « alerte reçue » ────────────────────────┘
                                      (prévu Phase 1)
```

1. Le coordinateur envoie un message depuis l'application Meshtastic (DM ou canal **Alertes**).
2. Le pager reçoit le paquet LoRa, le déchiffre et le traite *(traitement alerte : à venir)*.
3. L'opérateur voit l'alerte à l'écran, entend le buzzer et acquitte *(à venir)*.
4. Un accusé de réception est renvoyé en message direct *(à venir)*.

### Comportement actuel (implémenté)

| Composant | Comportement |
|:----------|:-------------|
| **Compilation** | Le flag `-D GAULIX_PAGER=1` est injecté uniquement pour `seeed_wio_tracker_L1`. |
| **GaulixPagerModule** | Instancié au démarrage si `GAULIX_PAGER && HAS_SCREEN`. |
| **Écran Etat_bipper** | Frame UI Meshtastic affichée dans la rotation d'écrans standard (trackball). |
| **Réseau** | Fonctionnement Meshtastic standard : mesh LoRa EU868, canaux préconfigurés au premier boot. |
| **Alertes** | `recordAlert()` existe mais n'est appelée par aucun autre module ; `wantPacket()` retourne `false`. |

Le pager se comporte donc aujourd'hui comme un nœud Meshtastic L1 Pro avec un **écran d'accueil dédié** et les **canaux Gaulix** en usine, en attendant le branchement de la logique `#alerte`.

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
| `src/modules/GaulixPagerModule.cpp` | Rendu OLED de l'écran d'état bippeur. |
| `docs/cahier-des-charges-bip-alerte-gaulix.md` | Spécification fonctionnelle. |
| `docs/propositions-phases-bip-gaulix.md` | Roadmap par phases. |
| `docs/BIPPER1.md` | Ce document. |

### Fichiers modifiés

| Fichier | Modification |
|:--------|:-------------|
| `variants/nrf52840/seeed_wio_tracker_L1/platformio.ini` | Ajout de `-D GAULIX_PAGER=1`. |
| `src/modules/Modules.cpp` | `#include` et `new GaulixPagerModule()` sous `#if defined(GAULIX_PAGER) && HAS_SCREEN`. |
| `userPrefs.jsonc` | Canaux Gaulix, région EU868, sonnerie RTTTL. |
| `src/mesh/Channels.cpp` | Support `USERPREFS_CHANNEL_3_*` et `USERPREFS_CHANNEL_7_*` + `initDefaultChannel(7)`. |
| `src/mesh/NodeDB.cpp` | Clés fork `USERPREFS_CONFIG_LORA_OK_TO_MQTT`, `USERPREFS_CONFIG_DEVICE_REBROADCAST_MODE`, `USERPREFS_CONFIG_DEVICE_NODE_INFO_BROADCAST_SECS` ; nom long Gaulix sous `GAULIX_PAGER`. |
| `userPrefs.jsonc` | Configuration usine complète Gaulix (LoRa, canaux, rôle, BT, GPS, position). |

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
| Nom long | `42BIP_LM8CMN-SDIS/XXXX` | `GAULIX_PAGER` : suffixe `%04x` = 4 derniers chiffres hex du node num |
| Nom court | défaut Meshtastic | `%04x` du node num (non surchargé) |

> **Limite 24 octets** (`MAX_LONG_NAME_BYTES`) : le chemin complet `42BIP_LM8CMN-SDIS/Dept/AASC/ODM/CRF/...` ne tient pas en usine — à compléter via l'app Meshtastic.

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
| Messages | *(placeholder)* | **App** — pas de clé `USERPREFS_*` ; à configurer dans l'app |

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

### Disposition à l'écran (OLED)

```
┌─────────────────────────────┐
│     PAGER Gaulix v1.0       │  ← titre centré (FONT_MEDIUM)
│  ● En écoute                │  ← pastille + libellé (FONT_SMALL)
│  Alerte(s) : 0   Dernière : │  ← compteur (gauche) + heure (droite)
│                    --:--    │
│  Batterie : 78 %            │  ← ou « USB », « -- », « 45 % + » si charge
└─────────────────────────────┘
```

| Zone | Source code | Détail |
|:-----|:------------|:-------|
| Titre | `drawFrame()` ligne 76 | `"PAGER Gaulix v1.0"` |
| Pastille verte | `fillCircle()` | Indicateur visuel « en écoute » (statique pour l'instant) |
| Statut | `"En écoute"` | Pas encore de machine d'états alerte |
| Compteur | `alertCount` | Incrémenté par `recordAlert()` *(non branché)* |
| Dernière alerte | `lastAlertTime` | Format `%H:%M` via `localtime()` ; `--:--` si aucune |
| Batterie | `powerStatus` | Pourcentage, suffixe `+` si en charge, `USB` si alimenté |

### API publique (préparée)

```cpp
static uint32_t getAlertCount();
static uint32_t getLastAlertTime();
static void recordAlert();  // alertCount++, lastAlertTime = getTime()
```

Ces méthodes seront appelées par le futur gestionnaire de commandes `#alerte` dans `handleReceived()`.

### Navigation

L'écran Etat_bipper apparaît dans le carrousel standard Meshtastic (trackball haut/bas sur le L1 Pro). Il n'est pas encore défini comme écran par défaut au démarrage.

---

## Matériel — Seeed Wio Tracker L1 Pro

| Ressource | Détail | Usage pager |
|:----------|:-------|:------------|
| MCU | nRF52840 | Exécution firmware, bootloader UF2 |
| Radio | SX1262 LoRa | Réseau Gaulix EU868 |
| Écran | OLED SSD1306 | Etat_bipper + alertes plein écran *(à venir)* |
| Buzzer | D12 (`PIN_BUZZER`) | Alarmes sonores *(à venir)* |
| LED verte | PIN_LED1 | Signal visuel *(à venir)* |
| LED bleue | PIN_LED2 | Signal alerte active *(à venir)* |
| Trackball + bouton | TB_* / D13 | Navigation et acquittement *(à venir)* |
| GPS L76K | UART | Position dans l'ACK *(à venir)* |

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
| Binaire UF2 | `.pio/build/seeed_wio_tracker_L1/firmware.uf2` |
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
4. Faire défiler les écrans OLED jusqu'à l'écran **PAGER Gaulix v1.0**.

---

## Fonctionnalités planifiées

Roadmap détaillée : [propositions-phases-bip-gaulix.md](propositions-phases-bip-gaulix.md).

### Phase 1 — Pager secours *(en cours)*

| Fonction | Statut | Description |
|:---------|:------:|:------------|
| Écran Etat_bipper | ✅ | Accueil pager avec batterie et compteurs |
| Canaux Gaulix par défaut | ✅ | ch0–3 + ch7 via userPrefs |
| `#alerte <texte>` | ⏳ | Déclenchement alerte secours |
| `#secours <texte>` | ⏳ | Synonyme alerte |
| `#fin` | ⏳ | Fin d'alerte, retour normal |
| Buzzer (3 bips / continu) | ⏳ | `#b <n>` pour régler le nombre |
| Écran plein écran alerte | ⏳ | « ALERTE SECOURS » + texte + horodatage |
| LEDs clignotantes | ⏳ | Séries de 3 impulsions |
| Acquittement trackball / bouton | ⏳ | Arrêt alarme |
| ACK automatique en DM | ⏳ | `Pager OK — alerte reçue à HH:MM` |
| Code d'activation | ⏳ | `#code ANCIEN NOUVEAU` |
| `#status` | ⏳ | État du pager à distance |
| Fiche réflexe opérateur | ⏳ | PDF 1 page |

### Phase 2 — Niveaux d'alerte *(V1.5)*

- `#info`, `#urgence` avec comportements sonores et visuels distincts
- Messages prédéfinis (CannedMessage)
- Liste blanche des coordinateurs
- Journal local des 20 dernières alertes
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

## Commandes prévues (référence)

| Commande | Action | Exemple |
|:---------|:-------|:--------|
| `#alerte <texte>` | Déclenche une alerte secours | `#alerte Rassemblement hall sportif` |
| `#secours <texte>` | Synonyme | `#secours Renforts secteur Nord` |
| `#fin` | Fin d'alerte | `#fin` |
| `#b <n>` | Nombre de bips (`0` = continu) | `#b 5` |
| `#code <ancien> <nouveau>` | Change le code d'activation | `#code GAULIX GAULIX26` |
| `#status` | État du pager | `#status` |

> Ces commandes sont spécifiées dans le cahier des charges ; **aucune n'est encore traitée** par le firmware Bipper1.

---

## Tests et validation

### Vérifications manuelles actuelles

| Test | Résultat attendu |
|:-----|:-----------------|
| Build `seeed_wio_tracker_L1` | Compilation sans erreur, `firmware.uf2` généré |
| Boot après flash UF2 | Nœud Meshtastic visible dans l'app |
| Canaux (après factory reset) | `Fr_Balise` (primary), `Fr_EMCOM`, `Fr_BlaBla`, `Fr_Tech`, `Alerte` |
| Écran pager | Frame « PAGER Gaulix v1.0 » dans la rotation OLED |
| Envoi `#alerte test` | **Aucun comportement spécial** (attendu tant que Phase 1 incomplète) |

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
| 1.1 | 06/07/2026 | Réseau Gaulix | Configuration usine complète (LoRa, canaux, rôle, BT, GPS) |
| 1.0 | 06/07/2026 | Réseau Gaulix | Documentation initiale de l'état fork Bipper1 |

---

*Fork maintenu par [F4EED/Bipper_L1Pro](https://github.com/F4EED/Bipper_L1Pro) — base [meshtastic/firmware](https://github.com/meshtastic/firmware).*
