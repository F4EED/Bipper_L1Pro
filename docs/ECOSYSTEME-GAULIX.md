# Écosystème Gaulix Bipper

Vue d’ensemble des trois briques du projet **pager d’alerte secours** Gaulix.

| Brique | Dépôt | Chemin local | Doc principale |
|:-------|:------|:-------------|:---------------|
| **Firmware** (Bipper) | [F4EED/Bipper_L1Pro](https://github.com/F4EED/Bipper_L1Pro) | `C:\firmware_meshtastic` | [BIPPER1.md](BIPPER1.md) |
| **Client web** | [F4EED/client_web_MT_bipper](https://github.com/F4EED/client_web_MT_bipper) | `C:\client web mesthastic_bipper` | `docs/BIPPER-WEB.md` |
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
| Seeed XIAO ESP32-S3 + Wio-SX1262 | `seeed-xiao-s3-gaulix` | Radio tête de réseau / coordinateur (sans écran, sans GPS UI) — clients web/Android en USB / Wi-Fi / BLE |

Build : `pio run -e seeed-xiao-s3-gaulix`. Nom usine : **Gaulix PC Crise**. Même bande EU868 / canaux Gaulix que les bippers ; rebroadcast **ALL** + Wi-Fi activé.

> **Règle projet** : firmware, web et Android **évoluent ensemble** (protocole, tags, docs). Voir `.cursor/rules/gaulix-ecosystem-sync.mdc` dans chaque dépôt.

## Versions alignées (juillet 2026)

| Composant | Version / état |
|:----------|:---------------|
| Firmware pager | **v1.11.0** (`GAULIX_PAGER_VERSION`) |
| Protocole filaire | `#alerte\|#secours\|#vigilance\|#info [N] <texte> [#entité…]` · `#fin [N] [#entité]` |
| Canal alertes | **Alerte** (index 7 usine) |
| Canal SOS / ACK position | **Fr_Balise** |
| Tags service (appartenance) | **T1–T10** |
| Multi-entités | Plusieurs `#entité` = **OU** |
| Nº d’alerte | Optionnel ; `#fin N` clôture uniquement N |

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

## Écran alerte Bipper (v1.11)

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
| Nº alerte + multi-tags + T1–T10 + écran L1–L6 | ✅ v1.11 | ✅ aligné | ✅ aligné |
| ThinkNode M1 / M2 (`GAULIX_PAGER`) | ✅ | ✅ détection HW | ✅ détection HW |
| XIAO ESP32-S3 + Wio-SX1262 (`seeed-xiao-s3-gaulix`, PC crise) | ✅ | USB/BLE/Wi-Fi | USB/BLE |
| Bouton SOS → waypoint Fr_Balise | ⏳ | affichage carte | ⏳ |
| Page / écran Signaler POI | — | ⏳ | ⏳ |

## Chemins locaux (dev)

| Rôle | Chemin typique Windows |
|:-----|:----------------------|
| Firmware | `C:\firmware_meshtastic` |
| Client web | `C:\client web mesthastic_bipper` |
| App Android | `C:\bipper_android` |
