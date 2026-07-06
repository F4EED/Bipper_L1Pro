---
title: "Propositions et phases — Bip alerte Gaulix"
version: "0.0.1"
date: "06/07/2026"
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
| **Version** | 0.0.1 |
| **Date** | 06/07/2026 |
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

### Ce qui manque aujourd'hui

- Pas de **mode pager secours** unifié
- Pas de **code d'activation** anti-fausses alertes
- Pas d'**acquittement** structuré avec retour au coordinateur
- Pas d'**écran d'accueil** dédié au rôle pager
- Pas de **commandes à distance** (`#alerte`, `#b`, `#fin`, etc.)

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

### Phase 1 — Pager secours Gaulix *(priorité immédiate)*

**Objectif :** livrer un bippeur d'alerte opérationnel sur L1 Pro.

#### Déclenchement

| Méthode | Exemple |
|:--------|:--------|
| Message avec mot-clé | `#alerte Rassemblement hall sportif` |
| Synonyme secours | `#secours Renforts secteur Nord` |
| Message direct ou canal PCS | Selon configuration réseau Gaulix |
| Code d'activation | `#alerte GAULIX <texte>` *(code configurable)* |

#### Comportement à la réception

1. Vérification du **code d'activation** (stocké en NVS, persistant)
2. **Écran plein** : « ALERTE SECOURS » + texte + horodatage
3. **3 bips** par défaut (pattern réglable)
4. **LEDs** : clignotement par séries de 3 impulsions
5. **Mode continu** (`#b 0`) : alarme jusqu'à acquittement
6. **Acquittement** : appui trackball ou bouton Program
7. **ACK automatique** en DM : `Pager OK — alerte reçue à HH:MM`
8. **Compteur** d'alertes + écran d'accueil pager

#### Commandes à distance

| Commande | Action |
|:---------|:-------|
| `#alerte <texte>` | Déclenche une alerte secours |
| `#secours <texte>` | Synonyme alerte |
| `#fin` | Fin d'alerte, retour à l'état normal |
| `#b <n>` | Règle le nombre de bips (`0` = continu) |
| `#code <ancien> <nouveau>` | Change le code d'activation |
| `#status` | État du pager (alertes, batterie, écoute) |

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
| **Info** | `#info <texte>` | 1 bip court | Bandeau discret |
| **Alerte** | `#alerte <texte>` | 3 bips | Plein écran orange |
| **Urgence** | `#urgence <texte>` | Alarme continue | Plein écran rouge clignotant |
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
┌─────────────────────┐
│  PAGER GAULIX v1.0  │
│  ● En écoute        │
│  Alertes : 12       │
│  Dernière : 14:32   │
│  Batterie : 78 %    │
└─────────────────────┘
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
| LED verte | PIN_LED1 | Signal « en écoute » |
| LED bleue | PIN_LED2 | Signal « alerte active » |
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
| Par où commencer ? | **Option B — Phase 1** |
| Matériel ? | **Seeed Wio Tracker L1 Pro** |
| Réseau ? | **Gaulix**, presets Meshtastic EU868 standards |
| Émission alertes ? | **App Meshtastic** (DM ou canal PCS) |
| Code d'activation ? | À définir *(ex. nom de commune ou code AASC local)* |
| Qui déclenche ? | Coordinateurs AASC / référents PCS *(liste blanche en Phase 2)* |
| Premier livrable ? | Firmware `.uf2` + fiche réflexe |

---

## Prochaines décisions à prendre

Avant de lancer le développement Phase 1, il reste à trancher :

1. **Code d'activation Gaulix** par défaut *(ex. `GAULIX`, `PCS2026`, nom de commune)*
2. **Canal broadcast** ou **messages directs** uniquement ?
3. **Liste blanche** dès la V1 ou seulement en Phase 2 ?
4. **Nom du module** firmware : `GaulixPagerModule` ou autre ?

---

<br>

<table width="100%">
<tr>
<td align="left"><em>Document complémentaire du cahier des charges V0.0.1</em></td>
<td align="right"><em>Réseau Gaulix — 06/07/2026</em></td>
</tr>
</table>
