---
title: "Propositions et phases — Bip alerte Gaulix"
version: "0.1.0"
date: "13/07/2026"
projet: "Réseau Gaulix · Meshtastic"
---

<table>
<tr>
<td width="140" valign="top">

![Logo Gaulix](gaulix.png)

</td>
<td valign="top">

# Propositions et phases recommandées

## Bip alerte secours — Réseau Gaulix

| | |
|:--|:--|
| **Document** | Roadmap fonctionnelle et technique |
| **Version** | 0.1.0 — **Phase 1 + appartenance (firmware v1.10.0)** |
| **Date** | 13/07/2026 |
| **Matériel cible** | Seeed Wio Tracker L1 Pro |
| **Stack** | Firmware Meshtastic (`seeed_wio_tracker_L1`) |

</td>
</tr>
</table>

---

## Vision du projet

Transformer un terminal **Meshtastic** en **pager d'alerte secours** pour le réseau **Gaulix** : à réception d'une commande d'activation, l'appareil déclenche un **bip sonore**, un **affichage plein écran** et un **signal visuel**, puis permet l'**acquittement** par l'opérateur et renvoie un **accusé de réception** au coordinateur.

> **Cadre d'usage visé :** secours citoyen, **AASC**, **PCS**, résilience communale — sans dépendance à des outils tiers, sans paramétrage radio spécifique, sur les presets Meshtastic standards (EU868).

---

## État des lieux

### Ce que Meshtastic offre déjà

| Brique existante | Usage possible |
|:-----------------|:---------------|
| Buzzer (`PIN_BUZZER`, D12 sur L1 Pro) | Bips, mélodies (`playBeep`, `playLongBeep`) |
| `ExternalNotificationModule` | Alerte sur message ou caractère cloche (`\x07`) |
| `screen->startAlert()` | Affichage plein écran temporaire |
| LEDs verte et bleue | Signal visuel complémentaire |
| Trackball + bouton Program | Acquittement local |
| GPS L76K | Position dans l'accusé de réception |
| Port `ALERT_APP` (protocole) | Canal dédié alertes critiques *(non implémenté en module firmware)* |
| Priorité réseau | Les paquets critiques passent avant les messages courants |

### Ce qui manquait — état juillet 2026 (v1.10.0)

| Besoin initial | Statut v1.10 |
|:---------------|:------------|
| Mode pager secours unifié | ✅ `GaulixPagerModule` |
| Commandes `#alerte`, `#fin`, `#status`, etc. | ✅ Whitelist + `#vigilance` |
| Appartenance / ciblage T1–T4 | ✅ `#cmd texte #entité` |
| Acquittement + ACK coordinateur | ✅ ACK DM + GPS Fr_Balise |
| Écran d'accueil pager | ✅ 4 lignes (`Nb AL.` / `Der.`, version, batterie) |
| Historique alertes local | ✅ `GaulixPagerAlertListModule` (20 entrées) |
| Alarme batterie faible | ✅ Bip doux à 10 %, rappel 5 min |
| Carrousel UI épuré | ✅ Node / Bearings / LoRa / favoris masqués |
| Clients coordinateurs (web + Android) | ✅ forks Gaulix |
| Code d'activation dans la syntaxe `#alerte` | ⏳ `#code` seul ; pas de code obligatoire dans `#alerte` |
| `#urgence`, liste blanche coordinateurs | ⏳ suite Phase 2 |

---

## Trois options de démarrage

### Option A — Rapide *(sans développement)*

Activer la configuration Meshtastic existante :

- Module **External Notification** activé
- Buzzer sur réception de message ou caractère cloche
- Envoi depuis l'app Meshtastic d'un message contenant `\x07`

| Avantages | Limites |
|:----------|:--------|
| Mise en place en ~30 min | Pas d'acquittement |
| Aucun code à maintenir | Pas de code secret |
| Compatible réseau Gaulix immédiat | Pas d'écran d'alerte dédié |
| | Pas de commandes structurées |

**Verdict :** suffisant pour un test, insuffisant pour un outil de crise fiable.

---

### Option B — Recommandée *(Phase 1 complète)*

Créer un module dédié **`GaulixPagerModule`** (ou `CrisisPagerModule`) dans le firmware Meshtastic, avec configuration par défaut pour le L1 Pro.

| Avantages | Effort estimé |
|:----------|:--------------|
| Reproduit l'essentiel d'un pager secours professionnel | ~500–800 lignes C++ |
| Compatible app Meshtastic native | 2–3 jours de développement + tests |
| Base solide pour les phases suivantes | 1 compilation firmware `.uf2` |

**Verdict :** **point de départ recommandé** pour le réseau Gaulix.

---

### Option C — Fork long terme *(vision complète)*

Branche firmware dédiée `gaulix-secours` avec UI, rôles, supervision web, statuts bénévoles, etc.

| Avantages | Effort estimé |
|:----------|:--------------|
| Outil de gestion de crise complet | Plusieurs semaines / mois |
| Différenciation forte du firmware standard | Maintenance continue |

**Verdict :** objectif des phases 3 et 4, pas du premier livrable.

---

## Phases recommandées

### Vue d'ensemble

```
Phase 1 ──► Phase 2 ──► Phase 3 ──► Phase 4
Pager       Niveaux     Réseau      Interface
secours     d'alerte    de crise    mode crise
(V1)        (V1.5)      (V2)        (V3)
```

---

### Phase 1 — Pager secours Gaulix *(implémentée — v1.6)*

**Objectif :** livrer un bippeur d'alerte opérationnel sur L1 Pro. **Statut : livré en firmware v1.6** (juillet 2026).

#### Déclenchement

| Méthode | Exemple |
|:--------|:--------|
| Alerte générale | `#alerte Rassemblement hall sportif` |
| Synonyme secours | `#secours Renforts secteur Nord` |
| Alerte par service | `#T1 Intervention` *(si `#tag 1` configuré localement)* |
| Info (sans écran alerte) | `#info Fin exercice` |
| DM ou canal **Alerte** | Broadcast ou message direct |

#### Comportement à la réception (v1.6)

1. **Whitelist** des commandes (`#alerte`, `#secours`, `#fin`, etc.) — texte libre ignoré
2. **Écran plein** : « ALERTE SECOURS » + texte + `JJ/MM HH:MM`
3. **Pim-pom** (3100 Hz / 2400 Hz) en boucle toutes les 1,5 s jusqu'à acquittement
4. **LED** PIN_LED1 : clignotement pendant alerte
5. **Acquittement** : appui trackball → ACK DM + bip fin
6. **ACK** : `Pager ACK alerte JJ/MM HH:MM` (+ GPS si activé)
7. **Écran accueil** : nom \| `Nb AL. : N \| Der. : HH:MM` \| version \| batterie
8. **Historique alertes** : frame 2, 20 entrées, scroll Haut/Bas
9. **Alarme batterie** : bip doux à ≤ 10 % (hors USB), rappel 5 min
10. **Timeout** : coupure auto après **30 min** sans acquittement

#### Commandes à distance (v1.6)

| Commande | Action |
|:---------|:-------|
| `#alerte <texte>` | Alerte secours (tous Bippers) |
| `#secours <texte>` | Synonyme |
| `#T1`…`#T4 <texte>` | Alerte si tag local correspond |
| `#tag <0-4>` | Configure le tag service |
| `#info` / `#Info <texte>` | 3× pim-pom, pas d'alerte écran |
| `#fin` | Fin d'alerte à distance |
| `#b <n>` | NVS legacy *(n'affecte plus le son)* |
| `#code <ancien> <nouveau>` | Code d'activation |
| `#status` | État du pager |

#### Sécurités

- Code d'activation obligatoire pour déclencher une alerte
- Coupure automatique de l'alarme continue après **30 min** sans acquittement
- Canal PCS / secours séparé du trafic social du mesh *(recommandé)*

#### Livrables Phase 1

| Livrable | Format |
|:---------|:-------|
| Firmware pager L1 Pro | `.uf2` |
| Fiche réflexe opérateur | PDF 1 page |
| Guide flashage | Markdown |
| Compte-rendu de tests réseau Gaulix | Markdown |

---

### Phase 2 — Niveaux d'alerte et messages prédéfinis *(V1.5)*

**Objectif :** enrichir le pager pour un usage opérationnel AASC / PCS.

#### Niveaux d'alerte

| Niveau | Commande | Son | Écran |
|:-------|:---------|:----|:------|
| **Info** | `#info <texte>` | 3× pim-pom | Pas d'écran alerte ✅ *v1.6* |
| **Alerte** | `#alerte <texte>` | Pim-pom continu | Plein écran ALERTE SECOURS ✅ *v1.6* |
| **Urgence** | `#urgence <texte>` | — | ⏳ Phase 2 |
| **Fin** | `#fin` | 2 bips descendants | Retour normal |

#### Fonctions additionnelles

- **Messages prédéfinis** (CannedMessage) : « Évacuation », « Point de rassemblement », « Fin d'exercice »
- **Liste blanche** : seuls les nœuds coordinateurs peuvent déclencher `#urgence`
- **Journal local** : 20 dernières alertes consultables sur l'écran d'accueil
- **Mode exercice** vs **mode réel** : distinguer entraînement et situation réelle

---

### Phase 3 — Outil réseau de crise *(V2)*

**Objectif :** exploiter la force du mesh Gaulix au-delà du bip individuel.

| Fonction | Description |
|:---------|:------------|
| **Carte des volontaires** | Positions GPS des nœuds équipés |
| **Statuts bénévoles** | « Disponible », « En route », « Sur place », « Indisponible » |
| **Capteurs** | Détection mouvement, porte, inondation → alerte automatique |
| **Store & Forward** | Alertes reçues même si le pager était hors couverture |
| **Pont MQTT** | Relais vers un poste de supervision (PC-Crise, mairie) |
| **Port `ALERT_APP`** | Implémentation firmware du canal alertes critiques Meshtastic |

#### Architecture cible

```
┌─────────────────┐     LoRa Gaulix      ┌──────────────────┐
│  Coordinateur   │ ───────────────────► │  Pagers secours  │
│  (app / PC)     │                      │  (L1 Pro × N)    │
└────────┬────────┘                      └────────┬─────────┘
         │                                        │
         │         ┌──────────────────┐           │
         └────────►│ Supervision web  │◄──────────┘
                   │ (optionnel V2)   │
                   └──────────────────┘
```

---

### Phase 4 — Interface « mode crise » dédiée *(V3)*

**Objectif :** transformer le L1 Pro en terminal de coordination léger.

#### Écran d'accueil pager

```
┌─────────────────────────────┐
│ Bipper de demo              │
│ Nb AL. : 0 | Der. : --:--   │
│      Bipper Gaulix v1.6     │
│ Batterie : 78 %             │
└─────────────────────────────┘
```

#### Navigation trackball

| Action | Résultat |
|:-------|:---------|
| Haut / Bas | Parcourir l'historique des alertes |
| Appui | Acquitter l'alerte en cours |
| Gauche | Envoyer son statut (« En route ») |
| Menu | Messages prédéfinis PCS |

---

## Matériel — Seeed Wio Tracker L1 Pro

| Ressource | Broche / détail | Usage crise |
|:----------|:----------------|:------------|
| Buzzer | D12 (`PIN_BUZZER`) | Alarmes sonores |
| LED alerte | PIN_LED1 | Clignotement alerte active |
| LED bleue | PIN_LED2 | **Ne pas utiliser pour alerte** (conflit buzzer corrigé en v1.3) |
| Trackball | TB_UP/DOWN/LEFT/RIGHT/PRESS | Navigation et acquittement |
| Bouton Program | D13 | Acquittement alternatif |
| Écran OLED | SSD1306 | Alertes plein écran |
| GPS L76K | UART | Position dans l'ACK |
| LoRa SX1262 | SPI | Réseau Gaulix |

**Compilation :** `python -m platformio run -e seeed_wio_tracker_L1`  
**Flashage :** fichier `.uf2` via bootloader USB (double-clic reset)

---

## Synthèse des recommandations

| Question | Recommandation |
|:---------|:---------------|
| Par où commencer ? | **Phase 1 livrée** — maintenance et Phase 2 |
| Matériel ? | **Seeed Wio Tracker L1 Pro** |
| Réseau ? | **Gaulix**, presets Meshtastic EU868 standards |
| Émission alertes ? | **App Meshtastic** (DM ou canal PCS) |
| Code d'activation ? | `GAULIX` par défaut — modifiable via `#code` |
| Premier livrable ? | Firmware `.uf2` v1.6 ✅ — fiche réflexe ⏳ |

---

## Prochaines décisions / travaux

1. **Fiche réflexe opérateur** PDF (envoi alerte, acquittement, test `#info`)
2. **Code d'activation** obligatoire dans la syntaxe `#alerte` ?
3. **Liste blanche** coordinateurs (Phase 2)
4. **`#urgence`** et niveaux visuels distincts

---

<br>

<table width="100%">
<tr>
<td align="left"><em>Document complémentaire du cahier des charges V0.1.0</em></td>
<td align="right"><em>Réseau Gaulix — 13/07/2026 — firmware v1.6</em></td>
</tr>
</table>
