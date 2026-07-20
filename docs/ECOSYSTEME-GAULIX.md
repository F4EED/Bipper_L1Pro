# Écosystème Gaulix Bipper

Vue d’ensemble des trois briques du projet **pager d’alerte secours** Gaulix.

| Brique | Dépôt | Doc principale |
|:-------|:------|:---------------|
| **Firmware** (Bipper L1 Pro) | [F4EED/Bipper_L1Pro](https://github.com/F4EED/Bipper_L1Pro) | [BIPPER1.md](BIPPER1.md) |
| **Client web** | [F4EED/client_web_MT_bipper](https://github.com/F4EED/client_web_MT_bipper) | `docs/BIPPER-WEB.md` |
| **App Android** | [F4EED/bipper_android](https://github.com/F4EED/bipper_android) | `docs/BIPPER-ANDROID.md` |

## Versions alignées (juillet 2026)

| Composant | Version / état |
|:----------|:---------------|
| Firmware pager | **v1.10.0** (`GAULIX_PAGER_VERSION`) |
| Protocole filaire | `#alerte\|#secours\|#vigilance\|#info\|#fin` + appartenance optionnelle `#entité` |
| Canal dédié | **Alerte** (index 7 usine) |
| Tags service | **T1–T4** (filtre d’appartenance à la réception) |

## Flux opérationnel

```
  Coordinateur (web /alerts ou app Android)
            │  texte mesh : #alerte … [#appartenance]
            ▼
     Canal Alerte / DM  ──LoRa Gaulix──►  Bipper L1 Pro (firmware)
            ▲                                      │
            └──────── Pager ACK (+ GPS) ───────────┘
```

1. Le coordinateur compose l’alerte (client web ou Android).
2. Le message part sur le canal **Alerte** (ou en DM).
3. Chaque Bipper filtre la whitelist + l’appartenance (T1–T4).
4. Son pim-pom + écran ; l’opérateur acquitte ; ACK renvoyé.

## Chemins locaux (dev)

| Rôle | Chemin typique Windows |
|:-----|:----------------------|
| Firmware | `C:\firmware_meshtastic` |
| Client web | `C:\client web mesthastic_bipper` |
| App Android | `C:\bipper_android` |
