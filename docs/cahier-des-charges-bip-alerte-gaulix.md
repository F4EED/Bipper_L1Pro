---
title: "Cahier des charges — Bip alerte Gaulix"
version: "0.1.0"
date: "13/07/2026"
author: "Réseau Gaulix"
status: "Implémenté — firmware v1.6 (Phase 1)"
---

<table>
<tr>
<td width="140" valign="top">

![Logo Gaulix](gaulix.png)

</td>
<td valign="top">

# Cahier des charges  
## Bip alerte

| | |
|:--|:--|
| **Document** | Cahier des charges Bip alerte — Réseau Gaulix |
| **Version** | 0.1.0 (spec) / firmware **v1.6** |
| **Date** | 13/07/2026 |

</td>
</tr>
</table>

---

## Contexte

Dans un contexte de multiplication des aléas climatiques, de tensions sur les réseaux de télécommunication classiques et de montée en puissance des dispositifs de **secours citoyen** et de **PCS** (Plan Communal de Sauvegarde), la capacité à **alerter rapidement** des bénévoles, des référents communaux ou des membres d'une structure de type **AASC** devient un enjeu opérationnel majeur.

Les solutions habituelles — SMS, groupes de messagerie, applications propriétaires — présentent des limites connues : dépendance à l'internet et au réseau mobile, saturation en situation de crise, absence de couverture en zone isolée, ou manque de matériel dédié pour les personnes qui doivent être **réveillées** ou **signalées immédiatement** (bip sonore, écran, acquittement).

Le réseau **Gaulix**, basé sur la technologie **Meshtastic** (communication LoRa maillée, décentralisée, longue portée, faible consommation), permet déjà l'échange de messages texte entre nœuds sans infrastructure centrale. Le firmware standard ne répond toutefois pas pleinement au besoin d'un **bippeur d'alerte secours** : déclenchement fiable, signal sonore et visuel fort, acquittement par l'opérateur, retour automatique au coordinateur.

L'objectif du présent cahier des charges est de définir la conception d'un **pager d'alerte Gaulix** : à réception d'une commande d'activation sur le réseau, l'appareil signale l'alerte de façon **sonore et visuelle**, affiche le message sur écran, et confirme la bonne réception à l'émetteur. La solution est pensée pour un usage **secours citoyen / AASC / PCS**, reproductible, sans dépendance à des outils tiers spécialisés et sans paramétrage radio hors standard Meshtastic.

<br>

> **Schéma de principe**

```
  Coordinateur                    Réseau LoRa Gaulix              Pager secours
  (app Meshtastic)         ──────────────────────────►         (Seeed L1 Pro)
        ▲                                                              │
        │                                                              │
        └──────────────── ACK « alerte reçue » ────────────────────────┘
```

<br>

![Terminal Seeed Wio Tracker L1 Pro — cible matérielle du pager Gaulix](images/l1-pro-pager.png)

*Seeed Wio Tracker L1 Pro : terminal retenu pour la V1 (écran OLED, buzzer, LEDs, trackball, GPS, LoRa SX1262).*

<br>

Le terminal cible dispose nativement des éléments nécessaires à un pager : **buzzer** pour l'alarme sonore, **écran OLED** pour l'affichage plein écran, **LEDs** pour un signal visuel complémentaire, **trackball et bouton** pour l'acquittement, et **GPS** pour enrichir l'accusé de réception. Le développement s'appuie sur le firmware officiel Meshtastic, avec un module logiciel dédié au mode pager, compilé pour la cible `seeed_wio_tracker_L1`.

L'émission des alertes se fait depuis l'**application Meshtastic** (Android / iOS) ou tout client compatible. Aucune application tierce n'est requise. Les coordinateurs AASC, référents PCS ou postes de coordination communaux peuvent ainsi déclencher une mise en alerte sur tout le réseau Gaulix ou en message direct vers un pager identifié.

---

## Recueil des besoins

Afin d'assurer la conception d'un bippeur d'alerte employable dans le cadre du réseau **Gaulix**, voici les points jugés prioritaires :

### Alerte et signalisation

- Réception d'une **commande d'activation** sur le réseau Meshtastic (message direct ou canal dédié Gaulix), avec **code d'activation** configurable pour limiter les fausses alertes.
- Syntaxe de commande simple et mémorisable, par exemple : `#alerte <texte>` ou `#secours <texte>`.
- À la réception d'une alerte valide :
  - **signal sonore** (buzzer) : séquence **pim-pom** (2 tons) répétée jusqu'à acquittement ; **3 séquences** pour `#info` ;
  - **affichage plein écran** sur OLED : libellé « ALERTE SECOURS », texte du message, horodatage `JJ/MM HH:MM` ;
  - **signal visuel** complémentaire (LED PIN_LED1) : clignotement pendant l'alerte.
- **Acquittement local** par l'opérateur (trackball) : arrêt alarme, retour écran d'accueil.
- **Accusé de réception automatique** en DM : `Pager ACK alerte JJ/MM HH:MM` (+ position GPS si activée).
- Commande de **fin d'alerte** (`#fin`) pour retour à l'état normal (sans ACK).
- **Écran d'accueil** pager (4 lignes) : nom long, `Nb AL. : N | Der. : HH:MM`, `Bipper Gaulix v1.6`, batterie.
- **Historique local** des 20 dernières alertes (frame carrousel 2, scroll Haut/Bas).
- **Alarme batterie faible** : bip sonore doux (moins fort que l'alerte) à **10 %** de charge, rappel périodique, silencieux en charge USB.
- Compteur d'alertes remis à **zéro** à chaque allumage.
- Tags de service `#T1`…`#T4` et `#tag` pour alertes ciblées.

### Réseau et usage opérationnel

- Compatibilité avec le **réseau Gaulix** et les **presets Meshtastic standards** (EU868, canaux habituels).
- Émission depuis l'**application Meshtastic** ; pas de dépendance à une plateforme propriétaire.
- Possibilité d'utiliser un **canal PCS / secours** distinct du trafic courant du mesh.
- Option future : liste blanche des nœuds autorisés à déclencher une alerte (coordinateurs AASC, référents PCS).

### Configuration à distance

- Réglage du **nombre de bips** (ex. `#b 5`) — enregistré en NVS ; **le son d'alerte v1.6 est toujours pim-pom continu** jusqu'à acquittement.
- Modification du **code d'activation** (ex. `#code ANCIEN NOUVEAU`), persistant après redémarrage.
- Borne de sécurité : coupure automatique de l'alarme continue après délai maximal (30 min) si aucun acquittement.

### Contraintes techniques et économiques

- Première cible matérielle : **Seeed Wio Tracker L1 Pro**.
- Livrable de flashage : fichier **`.uf2`** (bootloader nRF52).
- Module logiciel dédié isolé du reste du firmware pour faciliter la maintenance.
- Interface et messages en **français**, vocabulaire **secours / AASC / PCS**.
- Simplicité d'usage pour des bénévoles non techniciens.
- Coût matériel maîtrisé : terminal tout-en-un, pas de câblage externe obligatoire pour la V1.
- Reproductibilité : documentation de flashage et **fiche réflexe** « envoyer / tester une alerte ».

### Options et évolutions (hors périmètre V1)

- Niveaux d'alerte multiples (info / alerte / urgence).
- Statuts bénévoles (« disponible », « en route », « sur place »).
- Centralisation des acquittements via interface web ou poste de supervision.
- Pont MQTT vers une salle de crise ou un PC-Crise.
- Mode **exercice** vs **réel** pour les entraînements communaux.
- Intégration de capteurs (détection, porte, niveau d'eau, etc.).

<br>

![Exemple d'écran d'alerte sur le pager Gaulix](images/ecran-alerte-gaulix.png)

*Maquette d'affichage : alerte secours plein écran avec horodatage et libellé AASC.*

---

## Mise en œuvre

### Déploiement technique — Phase 1

| Étape | Contenu | Livrable |
|:-----:|:--------|:---------|
| 1 | Spécification détaillée des commandes et états | Présent document V0.0.1 |
| 2 | Développement du module pager sur firmware Meshtastic | Firmware `.uf2` L1 Pro |
| 3 | Tests sur réseau Gaulix (alerte, acquittement, ACK) | Compte-rendu de test |
| 4 | Rédaction fiche réflexe opérateur | PDF 1 page |
| 5 | Déploiement pilote (2 à 5 pagers) | Retour terrain |

### Promotion auprès des services concernés

Ce cas d'usage permettrait, d'une part, de **justifier et valoriser le réseau Gaulix** en proposant une solution concrète de **mise en alerte décentralisée**, et d'autre part de faciliter l'adhésion des communes, structures AASC et bénévoles en montrant un outil immédiatement utile en exercice comme en situation réelle.

Les communes, souvent responsables de la mise en œuvre du **PCS**, pourraient équiper des référents ou points de coordination d'un **pager Gaulix**, tout en déployant des nœuds relais Meshtastic pour couvrir le territoire (mairie, salle polyvalente, points hauts).

La solution reste **auto-hébergeable** au sens mesh : aucun abonnement, aucun serveur obligatoire pour la V1. Une évolution ultérieure pourrait ajouter un **poste de supervision** (interface web ou client PC) pour visualiser les acquittements et la position des bénévoles, notamment depuis un **PC-Crise** ou une mairie dotée d'une liaison internet (fixe ou satellite).

### Acteurs cibles

| Acteur | Rôle |
|:-------|:-----|
| Communes | Portage du PCS, financement relais, équipement référents |
| Structures AASC | Coordinateurs et bénévoles de secours citoyen |
| Réseau Gaulix | Déploiement, maintenance, formation |
| Bénévoles équipés | Réception, acquittement et mise en œuvre terrain |

### Critères de succès V1

- Une alerte `#alerte` envoyée depuis l'app Meshtastic déclenche bip + écran sur le pager en moins de **30 secondes** (selon topologie mesh).
- L'opérateur peut **acquitter** et l'émetteur reçoit un **ACK** explicite.
- Un non-initié peut utiliser le pager après **15 minutes** de formation.
- Le firmware se flashe et se reconfigure sans compétence développeur.

---

## Annexes

### A — Commandes implémentées (v1.6)

| Commande | Description | Exemple |
|:---------|:------------|:--------|
| `#alerte <texte>` | Alerte secours (tous Bippers) | `#alerte Rassemblement hall sportif` |
| `#secours <texte>` | Synonyme alerte | `#secours Renforts secteur Nord` |
| `#T1`…`#T4 <texte>` | Alerte si tag local = Tn | `#T1 Intervention` |
| `#tag <0-4>` | Tag service persistant | `#tag 1` |
| `#Info` / `#info <texte>` | 3× pim-pom, pas d'écran alerte | `#info Fin exercice` |
| `#fin` | Fin d'alerte, retour normal | `#fin` |
| `#b <n>` | Enregistre réglage NVS (legacy) | `#b 5` |
| `#code <ancien> <nouveau>` | Change le code d'activation | `#code GAULIX GAULIX26` |
| `#status` | État du pager | `#status` |

### B — Matériel cible V1

| Élément | Spécification |
|:--------|:--------------|
| Terminal | Seeed Wio Tracker L1 Pro |
| Processeur | nRF52840 |
| Radio | SX1262 (LoRa) |
| Affichage | OLED SSD1306 |
| Signalisation | Buzzer, 2 × LED, trackball |
| Géolocalisation | GPS L76K (optionnel dans ACK) |
| Flashage | Fichier `.uf2` via bootloader USB |

---

<br>

<table width="100%">
<tr>
<td align="left"><em>Révision #1</em></td>
<td align="right"><em>Révision #3 — 13/07/2026 — Implémentation v1.6</em></td>
</tr>
</table>
