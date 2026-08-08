# Écosystème Gaulix Bipper

Vue d’ensemble des trois briques du projet **pager d’alerte secours** Gaulix.

| Brique | Dépôt | Chemin local | Doc principale |
|:-------|:------|:-------------|:---------------|
| **Firmware** (Bipper) | [F4EED/Bipper_L1Pro](https://github.com/F4EED/Bipper_L1Pro) | `C:\firmware_meshtastic` | [BIPPER1.md](BIPPER1.md) |
| **Client web** | [F4EED/client_web_MT_bipper](https://github.com/F4EED/client_web_MT_bipper) | `C:\client web mesthastic_bipper` | `docs/BIPPER-WEB.md` · install PC : [`docs/install_local.md`](https://github.com/F4EED/client_web_MT_bipper/blob/main/docs/install_local.md) |
| **App Android** | [F4EED/bipper_android](https://github.com/F4EED/bipper_android) | `C:\bipper_android` | `docs/BIPPER-ANDROID.md` |

## Matériels pager supportés (`GAULIX_PAGER=1`)

| Matériel | Env PlatformIO | Arch | Écran | Buzzer |
|:---------|:---------------|:-----|:------|:-------|
| Seeed Wio Tracker L1 Pro | `seeed_wio_tracker_L1` | nRF52840 | OLED | oui |
| Seeed Wio Tracker L1 E-Ink | `seeed_wio_tracker_L1_eink` | nRF52840 | e-ink | oui |
| Elecrow ThinkNode M1 | `thinknode_m1` | nRF52840 | e-ink 1.54\" | oui (`PIN_BUZZER`) |
| Elecrow ThinkNode M2 | `thinknode_m2` | ESP32-S3 | OLED SH1106 | oui (`PIN_BUZZER`) |

> Variantes InkHUD (`thinknode_m1-inkhud`, `seeed_wio_tracker_L1_eink-inkhud`) : **pas** de mode Gaulix pager (UI incompatible).

## Nœud PC de crise (`GAULIX_PC_NODE=1`)

| Matériel | Env PlatformIO | Rôle |
|:---------|:---------------|:-----|
| Seeed XIAO ESP32-S3 + Wio-SX1262 | `seeed-xiao-s3-gaulix` | Radio tête de réseau / coordinateur (sans écran) — clients web/Android en USB / Wi-Fi / BLE |
| Elecrow ThinkNode M2 | `thinknode_m2-gaulix` | PC crise avec **OLED** (statut Meshtastic) ; pas de UI pager — `thinknode_m2` reste le bipper |

Builds : `pio run -e seeed-xiao-s3-gaulix` · `pio run -e thinknode_m2-gaulix`.

| Champ usine (`GAULIX_PC_NODE`) | Valeur |
|:-------------------------------|:-------|
| Nom long | **Gaulix PC Crise** |
| Nom court | **🔴** (cercle rouge) |
| Rôle | CLIENT |
| Rebroadcast | **LOCAL_ONLY** |
| Radio | EU868 / canaux Gaulix · Wi‑Fi activé |
| MQTT | **non** (rôle client USB/Wi‑Fi/BLE) — uplink broker via **passerelle MQTT dédiée** |

**Clients GerMaCrise** (web + Android) : icône mesh/télécom/crise orange `#E85D04` ; PWA web `short_name` = **🔴**. Portail : [germacrise.wordpress.com](https://germacrise.wordpress.com/).

**App Android** (`org.germacrise.app`, minSdk 26) : coexiste avec Meshtastic officiel. Install APK / dépannage (« Application non installée », Crosscall Core-X4 Android 10, `adb install`) → Android [`docs/BIPPER-ANDROID.md`](https://github.com/F4EED/bipper_android/blob/main/docs/BIPPER-ANDROID.md#installation-sur-téléphone).

### Signalement GerMaCrise — objets Alerte + Fr_Balise

Tous les boutons **Signalement** (web `/alerts`, Android GerMaCrise) envoient un **waypoint** (`PortNum.WAYPOINT_APP` = 8), GPS obligatoire, priorité mesh **`ALERT`** (110), en **double émission** :

1. **Alerte** (canal **7**) — pager / MQTT Gaulix  
2. **Fr_Balise** (canal **0**) — objets carte / mesh balises

Catégories UI (Routes / Status / SDIS / Secourisme / Crise / ADRASEC) : matrice Excel GerMaCrise (boutons multi-onglets) — même payload waypoint, seul le libellé / icône change.

Uplink MQTT : le **PC crise** (`GAULIX_PC_NODE`) n’active pas MQTT en usine. Une **passerelle MQTT dédiée** (autre nœud mesh avec Wi‑Fi + `module.mqtt.enabled`) doit entendre le LoRa et uplinker — voir checklist.

### Checklist diagnostic MQTT (passerelle dédiée)

Sur la **passerelle MQTT** (pas le PC crise coordinateur) :

1. **`module.mqtt.enabled` = true** + adresse / identifiants broker corrects.
2. **Wi‑Fi / lien broker** — SSID joignable ; éventuellement `proxy_to_client_enabled` si le client fournit le lien.
3. **Portée LoRa** — la passerelle doit recevoir les paquets Fr_Balise / Alerte émis depuis le PC crise ou les bippers.
4. **Uplink canaux** — `Alerte` (waypoints signalement + pager) : `settings.uplink_enabled` (usine Gaulix : oui via `USERPREFS_CHANNEL_*_UPLINK_ENABLED`).
5. **`config.lora.config_ok_to_mqtt`** sur l’émetteur — usine Gaulix `true` ; sinon un broker public peut filtrer (`DontMqttMeBro`).
6. **Test** — envoyer **Incendie** (waypoint Alerte) puis vérifier topic MQTT / logs série de la passerelle `MQTT onSend - Publish`.

**Client web USB** : API **Web Serial** — **Chrome** ou **Edge** (Firefox ≥ 151 possible ; Safari / Firefox plus anciens : *Web Serial not supported*). Détail : web `docs/BIPPER-WEB.md` § Navigateurs.

> **Règle projet** : firmware, web et Android **évoluent ensemble** (protocole, tags, docs). Voir `.cursor/rules/gaulix-ecosystem-sync.mdc` dans chaque dépôt.

## Versions alignées (juillet 2026)

| Composant | Version / état |
|:----------|:---------------|
| Firmware pager | **v1.12.3** (`GAULIX_PAGER_VERSION`) |
| Protocole filaire | `#alerte\|#secours\|#vigilance\|#info [N] <texte> [#entité…]` · `#fin [N] [#entité]` |
| ACK lecture | `Pager ACK alerte [#N] …` en **broadcast canal Alerte** (plus de DM PKI → Fr_BlaBla/Primary) |
| Canal alertes + waypoints signalement | **Alerte** (7) + **Fr_Balise** (0) — double TX PortNum 8 |
| Canal SOS / position ACK GPS | **Fr_Balise** |
| Tags service (appartenance) | **T1–T10** |
| Multi-entités | Plusieurs `#entité` = **OU** |
| Nº d’alerte | Optionnel ; `#fin N` clôture uniquement N |
| Nb AL. (écran) | Alertes non clôturées ; −1 saturé à 0 (#fin / ACK / timeout) |
| ThinkNode M1 buzzer | PWM duty **75 %** (`GAULIX_BUZZER_DUTY`) |

## Format filaire (source de vérité)

```text
#alerte 42 incendie hall #SDIS42 #test
#secours 42 renforts Nord #DEPT42
#vigilance 7 exercice #test
#info 7 fin exercice #test
#fin 42
#fin 42 #SDIS42
#fin
```

| Élément | Règle |
|:--------|:------|
| Type | `#alerte` / `#secours` / `#vigilance` / `#info` / `#fin` |
| `N` | Numéro d’alerte (entier, optionnel) |
| Texte | Mots libres (pas de `#` en tête de jeton) |
| `#entité` | Un ou plusieurs ; match si le bip a l’entité dans **n’importe quel** slot T1–T10 |
| Sans `#entité` | Broadcast : tous les bippers |
| `#fin` sans `N` | Clôture l’alerte active (compat) |
| `#fin N` | Clôture seulement si l’alerte active a le nº N |

Config appartenance locale :

```text
#tagset T1=SDIS42,T2=DEPT42,T3=Test,T4=UDIOM42,T5=,T6=,T7=,T8=,T9=,T10=
#tagval 3 Ricamarie
#status
```

## Écran alerte Bipper (v1.12)

| Ligne | Contenu |
|:------|:--------|
| L1 | Type (`ALERTE` / `SECOURS` / `VIGILANCE`) + `#N` si présent |
| L2 | Texte du message |
| L3 | Émetteur (nom long / court / `!nodeid`) |
| L4 | (réservé) |
| L5 | Date et heure de réception |
| L6 | « Appuyer pour confirmer lecture » |

## Flux opérationnel

```
  Coordinateur (web /alerts ou app Android)
            │  via PC crise XIAO ou autre nœud
            │  #alerte N texte #E1 #E2
            ▼
     Canal Alerte / DM  ──LoRa Gaulix──►  Bipper (L1 / ThinkNode M1 / M2)
            ▲                                      │
            └──────── Pager ACK (+ GPS Fr_Balise) ─┘
```

1. Le coordinateur compose l’alerte (web ou Android) avec nº + multi-tags.
2. Le message part sur le canal **Alerte** (ou en DM).
3. Chaque Bipper filtre whitelist + appartenance T1–T10 (OU).
4. Son pim-pom + écran L1–L6 ; acquittement ; ACK (+ position sur Fr_Balise).
5. Clôture distante : `#fin N`.

## Roadmap commune (prochaine)

| Item | Firmware | Web | Android |
|:-----|:---------|:----|:--------|
| Nº alerte + multi-tags + T1–T10 + écran L1–L6 | ✅ v1.12 | ✅ aligné | ✅ aligné |
| ThinkNode M1 / M2 (`GAULIX_PAGER`) + volume M1 75 % | ✅ | ✅ détection HW | ✅ détection HW |
| XIAO ESP32-S3 + Wio-SX1262 (`seeed-xiao-s3-gaulix`, PC crise) | ✅ | USB/BLE/Wi-Fi | USB/BLE |
| ThinkNode M2 (`thinknode_m2-gaulix`, PC crise) | ✅ | USB/BLE/Wi-Fi | USB/BLE |
| Gestion des alertes (Signalement / Message / Alertes / ACK) | ACK `#N` | ✅ `/alerts` (Signalement 1er) | ✅ (Signalement 1er) |
| Signalement POI → waypoint Alerte (icônes emoji) | — | ✅ onglet + carte | ✅ onglet + carte |
| Bouton SOS → waypoint Fr_Balise | ⏳ | affichage carte | ⏳ |

## Chemins locaux (dev)

| Rôle | Chemin typique Windows |
|:-----|:----------------------|
| Firmware | `C:\firmware_meshtastic` |
| Client web | `C:\client web mesthastic_bipper` |
| App Android | `C:\bipper_android` |
