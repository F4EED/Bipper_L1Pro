# Transfert du projet Gaulix Bipper vers un autre PC

| | |
|:--|:--|
| **Projet** | Firmware Meshtastic personnalisé — pager d'alerte Gaulix |
| **Dépôt local** | `C:\firmware_meshtastic` |
| **Remote** | [F4EED/Bipper_L1Pro](https://github.com/F4EED/Bipper_L1Pro) |
| **Branche de travail** | `develop` |
| **Matériels** | L1 Pro · ThinkNode M1/M2 · PC crise XIAO S3+SX1262 / ThinkNode M2 |
| **Document lié** | [BIPPER1.md](BIPPER1.md) · [ECOSYSTEME-GAULIX.md](ECOSYSTEME-GAULIX.md) |

Ce guide décrit comment reprendre le développement et le flashage des Bippers / PC crise sur une nouvelle machine Windows (32 Go RAM, NVMe, Cursor déjà installé).

---

## 1. Ce qu'il faut copier — et ce qu'il faut ignorer

### À transférer (obligatoire)

| Élément | Pourquoi |
|:--------|:---------|
| Le dépôt Git (`C:\firmware_meshtastic`) | Code source, variantes, module Gaulix |
| `userPrefs.jsonc` | Canaux Gaulix par défaut (Fr_Balise, Fr_EMCOM, Alerte ch7, LoRa EU868, etc.) |
| Modifications non poussées | Vérifier avec `git status` avant le transfert |

### À ne **pas** copier (régénérable)

| Dossier / fichier | Raison |
|:------------------|:-------|
| `.pio/` | Cache PlatformIO local (dépendances, objets intermédiaires) |
| `C:\pio-build\` | Répertoire de build externe (voir `build_dir` dans `platformio.ini`) |
| `upload-result.txt`, `upload-verbose.log` | Journaux de flash locaux |
| `.vscode/`, `.claude/` | Préférences IDE locales (optionnel) |

> **Règle d'or :** utilisez Git comme source de vérité. Les artefacts de compilation se recréent au premier build.

### Fichiers clés du fork Gaulix

| Fichier | Rôle |
|:--------|:-----|
| `src/modules/GaulixPagerModule.cpp` / `.h` | Module UI pager Gaulix + alarme batterie 10 % |
| `src/modules/GaulixPagerAlertListModule.cpp` / `.h` | Historique des 20 dernières alertes |
| `variants/nrf52840/seeed_wio_tracker_L1/platformio.ini` | Flag `-D GAULIX_PAGER=1` |
| `variants/nrf52840/ELECROW-ThinkNode-M1/platformio.ini` | Flag `-D GAULIX_PAGER=1` (`thinknode_m1`) |
| `variants/esp32s3/ELECROW-ThinkNode-M2/platformio.ini` | `thinknode_m2` (`GAULIX_PAGER`) · `thinknode_m2-gaulix` (`GAULIX_PC_NODE`) |
| `variants/esp32s3/seeed_xiao_s3/platformio.ini` | Env `seeed-xiao-s3-gaulix` (`GAULIX_PC_NODE=1`) |
| `platformio.ini` | `build_dir = C:/pio-build` (contournement chemins longs / verrous Windows) |
| `src/buzz/buzz.cpp` | Sons Gaulix : pim-pom alerte + `playGaulixLowBatteryBeep()` |
| `src/graphics/Screen.cpp` | Masquage pages Node / Bearings / LoRa / favoris (Gaulix) |
| `userPrefs.jsonc` | Canaux Gaulix, EU868, nom `Bipper de demo` |

---

## 2. Option A — Cloner depuis le remote (recommandé)

Méthode la plus propre si vos commits sont sur GitHub.

### Sur l'ancien PC — avant de partir

```powershell
cd C:\firmware_meshtastic
git status
git push origin develop
```

Poussez toute branche contenant du travail non sauvegardé.

### Sur le nouveau PC

```powershell
# Installer Git si nécessaire : https://git-scm.com/download/win

cd C:\
git clone https://github.com/F4EED/Bipper_L1Pro.git firmware_meshtastic
cd C:\firmware_meshtastic
git checkout develop

# Remote upstream Meshtastic (optionnel, pour rebases)
git remote add upstream https://github.com/meshtastic/firmware.git
```

Vérifiez que les fichiers Gaulix sont présents :

```powershell
Test-Path src\modules\GaulixPagerModule.cpp
Select-String -Path variants\nrf52840\seeed_wio_tracker_L1\platformio.ini -Pattern "GAULIX_PAGER"
Select-String -Path platformio.ini -Pattern "build_dir"
```

---

## 3. Option B — Transfert USB ou archive

Utile si vous avez des modifications locales non commitées ou pas d'accès réseau.

### Créer l'archive sur l'ancien PC

```powershell
cd C:\firmware_meshtastic

# Vérifier l'état Git
git status

# Créer une archive ZIP sans les dossiers lourds
$exclude = @('.pio', 'node_modules')
$dest = "$env:USERPROFILE\Desktop\firmware_meshtastic_transfer.zip"

# Méthode simple : copier vers clé USB en excluant manuellement
robocopy C:\firmware_meshtastic E:\firmware_meshtastic /E /XD .pio C:\pio-build .git\objects\pack /XF *.uf2 *.elf *.hex
```

> **Attention :** avec `robocopy`, adaptez la lettre du lecteur USB (`E:\`). Vous pouvez aussi copier tout le dossier puis supprimer `.pio` et `C:\pio-build` sur la clé pour gagner de l'espace.

Alternative ZIP (PowerShell 5+) :

```powershell
# Exclure .pio du zip en copiant d'abord vers un dossier temporaire filtré
$src = "C:\firmware_meshtastic"
$tmp = "$env:TEMP\firmware_meshtastic_clean"
robocopy $src $tmp /MIR /XD .pio
Compress-Archive -Path $tmp -DestinationPath "$env:USERPROFILE\Desktop\firmware_meshtastic.zip" -Force
```

### Restaurer sur le nouveau PC

```powershell
# Depuis clé USB
robocopy E:\firmware_meshtastic C:\firmware_meshtastic /E

# Ou depuis ZIP
Expand-Archive -Path "$env:USERPROFILE\Desktop\firmware_meshtastic.zip" -DestinationPath C:\
```

Puis réinitialiser le remote si besoin :

```powershell
cd C:\firmware_meshtastic
git remote -v
git remote set-url origin https://github.com/F4EED/Bipper_L1Pro.git
```

---

## 4. Prérequis sur le nouveau PC

### Obligatoires

| Outil | Installation |
|:------|:-------------|
| **Git** | [git-scm.com/download/win](https://git-scm.com/download/win) |
| **Python 3** | [python.org](https://www.python.org/downloads/) — cocher « Add to PATH » |
| **PlatformIO** | Via extension Cursor/VS Code **ou** CLI (voir ci-dessous) |

### PlatformIO — deux méthodes

**Méthode 1 — Extension (recommandée avec Cursor)**

1. Ouvrir Cursor → Extensions (`Ctrl+Shift+X`)
2. Installer **PlatformIO IDE**
3. Redémarrer Cursor si demandé
4. PlatformIO installe automatiquement son environnement Python isolé

**Méthode 2 — CLI pip**

```powershell
pip install platformio
pio --version
```

### Optionnel mais utile

| Outil | Usage |
|:------|:------|
| **trunk** | Formatage avant commit (`trunk fmt`) — voir [AGENTS.md](../AGENTS.md) |
| **Pilote USB série** | Pour le port COM du L1 Pro (souvent automatique sous Windows 10/11) |
| **meshtastic-mcp** | Flash/diagnostic avancé via MCP (optionnel) |

### Antivirus — exclusion recommandée

Ajoutez une exclusion Windows Defender (ou autre AV) pour :

- `C:\firmware_meshtastic`
- `C:\pio-build`

Les scans en temps réel sur des milliers de fichiers `.o` provoquent des **verrous de fichiers** et des builds très lents.

---

## 5. Configuration Cursor

```powershell
# Ouvrir le projet
cursor C:\firmware_meshtastic
```

Dans Cursor :

1. **Fichier → Ouvrir le dossier** → `C:\firmware_meshtastic`
2. Attendre l'indexation PlatformIO (barre d'état en bas)
3. Vérifier que l'extension PlatformIO est active
4. Terminal intégré : PowerShell (`Ctrl+``)

Le fichier `.vscode/tasks.json` local (s'il existe) peut contenir des tâches de build personnalisées ; il n'est pas requis pour compiler.

---

## 6. Premier build sur la nouvelle machine

### Environnements PlatformIO du projet

| Environnement | Écran | Module Gaulix |
|:--------------|:------|:--------------|
| `seeed_wio_tracker_L1` | OLED | **Oui** (`GAULIX_PAGER=1`) |
| `thinknode_m1` | E-ink 1.54\" (Elecrow) | **Oui** (`GAULIX_PAGER=1`) |
| `thinknode_m2` | OLED SH1106 (Elecrow ESP32-S3) | **Oui** (`GAULIX_PAGER=1`) |
| `seeed_wio_tracker_L1_eink` | E-ink L1 | **Oui** (`GAULIX_PAGER=1`) |
| `seeed-xiao-s3-gaulix` | aucun (XIAO S3 + Wio-SX1262) | **PC crise** (`GAULIX_PC_NODE=1`) |
| `thinknode_m2-gaulix` | OLED SH1106 (statut, pas pager) | **PC crise** (`GAULIX_PC_NODE=1`) |
| `thinknode_m1-inkhud` | InkHUD | **Non** (UI incompatible) |
| `seeed_wio_tracker_L1_eink-inkhud` | InkHUD | **Non** (UI incompatible) |

### Build complet — variante OLED Gaulix

```powershell
cd C:\firmware_meshtastic

# IMPORTANT sous Windows : -j 1 évite les builds parallèles qui verrouillent les fichiers
python -m platformio run -e seeed_wio_tracker_L1 -j 1
```

Le premier build télécharge les toolchains et bibliothèques — comptez **15 à 25 minutes** sur une machine 32 Go / NVMe. Les builds incrémentaux prennent quelques minutes.

### Sorties attendues

| Fichier | Chemin |
|:--------|:-------|
| UF2 (flash manuel) | `C:\pio-build\seeed_wio_tracker_L1\firmware.uf2` |
| Hex | `C:\pio-build\seeed_wio_tracker_L1\firmware.hex` |
| ELF | `C:\pio-build\seeed_wio_tracker_L1\firmware.elf` |

> Le `build_dir` est défini à la racine du disque (`C:/pio-build`) dans `platformio.ini` pour éviter les chemins trop longs et les conflits de verrous sous Windows.

### Build variante e-ink L1 (Gaulix pager)

```powershell
python -m platformio run -e seeed_wio_tracker_L1_eink -j 1
```

Sortie UF2 : `C:\pio-build\seeed_wio_tracker_L1_eink\firmware.uf2`

### Vérification rapide après build

```powershell
Get-ChildItem C:\pio-build\seeed_wio_tracker_L1\*.uf2
python -m platformio run -e seeed_wio_tracker_L1 -t envdump | Select-String "GAULIX"
```

---

## 7. Flashage des Bippers

### Prérequis matériel

- Seeed Wio Tracker L1 Pro
- Câble USB données (pas charge seule)
- Firmware UF2 compilé ou prêt à être uploadé

### Méthode 1 — UF2 (bootloader, sans PlatformIO)

1. Mettre le L1 Pro en **mode DFU / bootloader** :
   - **Double-appui rapide** sur le bouton RESET, **ou**
   - Envoyer un signal **1200 baud** sur le port série (voir dépannage).
2. Un lecteur USB apparaît (nom type **WIO Tracker** ou **XIAO**).
3. Copier le firmware :

```powershell
Copy-Item C:\pio-build\seeed_wio_tracker_L1\firmware.uf2 -Destination D:\
```

(`D:` = lettre du volume UF2 — adaptez selon l'Explorateur Windows.)

4. L'appareil redémarre automatiquement.

### Méthode 2 — PlatformIO upload (recommandée au quotidien)

```powershell
# Lister les ports COM disponibles
python -m platformio device list

# Flasher (remplacer COM7 par votre port)
python -m platformio run -e seeed_wio_tracker_L1 -j 1 -t upload --upload-port COM7
```

### Après le premier flash

1. Ouvrir l'application Meshtastic et associer le nœud.
2. Si l'appareil avait une ancienne config : **factory reset** pour appliquer les canaux Gaulix de `userPrefs.jsonc`.
3. Vérifier les canaux : `Fr_Balise`, `Fr_EMCOM`, `Fr_BlaBla`, `Fr_Tech`, `Alerte`.
4. Sur l'OLED : écran d'accueil **Bipper Gaulix v1.6** (nom, `Nb AL.` / `Der.`, batterie).

### Parc matériel de test (juillet 2026)

| Appareil | Port typique | S/N |
|:---------|:-------------|:----|
| Bipper 1 | COM20 | `681A8AEB0A2F672B` |
| Bipper 2 | COM10 | `D08260225E657DB5` |

```powershell
pio run -e seeed_wio_tracker_L1 -t upload --upload-port COM20
```

---

## 8. Checklist post-transfert

Exécutez ces vérifications avant de considérer le transfert terminé :

```powershell
cd C:\firmware_meshtastic

# 1. build_dir externe
Select-String -Path platformio.ini -Pattern 'build_dir = C:/pio-build'

# 2. Flag Gaulix sur la variante OLED
Select-String -Path variants\nrf52840\seeed_wio_tracker_L1\platformio.ini -Pattern "GAULIX_PAGER=1"

# 3. Module Gaulix présent
Test-Path src\modules\GaulixPagerModule.cpp
Test-Path src\modules\GaulixPagerModule.h

# 4. Canaux Gaulix dans userPrefs
Select-String -Path userPrefs.jsonc -Pattern "Fr_Balise|Fr_EMCOM|Alerte|EU_868"

# 5. Build de validation
python -m platformio run -e seeed_wio_tracker_L1 -j 1
```

### Contenu attendu de `userPrefs.jsonc` (extrait)

- Canaux : `Fr_Balise` (ch0), `Fr_EMCOM` (ch1), `Fr_BlaBla`, `Fr_Tech`, `Alerte` (ch7)
- Région LoRa : `EU_868` (forcée à chaque boot en v1.6)
- Nom usine : `Bipper de demo`
- Rôle : `CLIENT_MUTE`

---

## 9. Temps de build attendus

| Scénario | Durée indicative (32 Go RAM, NVMe) |
|:---------|:-------------------------------------|
| Premier build complet (`seeed_wio_tracker_L1`) | 15 – 25 min |
| Build incrémental (petite modification) | **1 – 2 min** |
| Changement de lib / clean build | 15 – 20 min |
| Variante e-ink (premier build) | 15 – 25 min |

Facteurs qui ralentissent : antivirus actif, build parallèle (`-j` > 1), disque HDD, connexion Internet lente (téléchargement toolchains).

---

## 10. Dépannage

### Erreur « file is being used by another process » / verrous Windows

```powershell
# Toujours compiler avec un seul job
python -m platformio run -e seeed_wio_tracker_L1 -j 1

# Nettoyer et reconstruire
python -m platformio run -e seeed_wio_tracker_L1 -t clean
python -m platformio run -e seeed_wio_tracker_L1 -j 1
```

- Fermer les moniteurs série (`pio device monitor`) avant de compiler.
- Exclure `C:\pio-build` et `C:\firmware_meshtastic` de l'antivirus.
- Ne pas lancer deux builds PlatformIO en parallèle.

### Chemins trop longs

Le projet utilise déjà `build_dir = C:/pio-build`. Si des erreurs persistent :

```powershell
# Vérifier que build_dir est bien pris en compte
python -m platformio run -e seeed_wio_tracker_L1 -t envdump | Select-String "build_dir"
```

### nRF52 ne répond plus / pas de port COM

1. Débrancher/rebrancher le câble USB.
2. Essayer un autre port USB (de préférence USB 2.0 direct, pas via hub).
3. **Touch 1200 baud** pour forcer le bootloader DFU :

```powershell
# Avec Python pyserial installé
python -c "import serial, time; s=serial.Serial('COM7', 1200); time.sleep(0.5); s.close()"
```

Remplacez `COM7` par le port détecté avant la panne.

4. Si disponible via meshtastic-mcp : outil `touch_1200bps`.
5. En dernier recours : double RESET rapide, puis copie UF2 manuelle.

### `pio` introuvable dans PowerShell

```powershell
# Utiliser le module Python (fonctionne même sans PATH)
python -m platformio --version

# Ou chemin PlatformIO de l'extension
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" --version
```

### Build OK mais pas d'écran Gaulix

- Confirmer que vous flashez `seeed_wio_tracker_L1` (pas `seeed_wio_tracker_L1_eink`).
- Vérifier `GAULIX_PAGER=1` dans la variante OLED.
- Factory reset si une ancienne config masque les canaux.

### Git — remote ou branche incorrecte

```powershell
git remote -v
# Attendu : origin → https://github.com/F4EED/Bipper_L1Pro.git

git branch
git checkout develop
git pull origin develop
```

---

## Résumé des commandes essentielles

```powershell
# Setup
git clone https://github.com/F4EED/Bipper_L1Pro.git C:\firmware_meshtastic
cd C:\firmware_meshtastic
git checkout develop

# Build Gaulix OLED
python -m platformio run -e seeed_wio_tracker_L1 -j 1

# Flash
python -m platformio device list
python -m platformio run -e seeed_wio_tracker_L1 -j 1 -t upload --upload-port COM7

# UF2 manuel
Copy-Item C:\pio-build\seeed_wio_tracker_L1\firmware.uf2 D:\
```

---

*Dernière mise à jour : 30/07/2026 — Gaulix Bipper **v1.11.0** (branche `develop`).*

### Nouveautés v1.11 (résumé)

| Fonction | Détail |
|:---------|:-------|
| Protocole | `#alerte [N] texte #E1 #E2` · `#fin N` · T1–T10 |
| Pagers | L1 Pro · ThinkNode M1 · ThinkNode M2 |
| PC crise | XIAO S3+SX1262 (`seeed-xiao-s3-gaulix`) · ThinkNode M2 (`thinknode_m2-gaulix`) |
| Version module | `GAULIX_PAGER_VERSION` = `v1.11.0` |
