# Hell's Tower

Jeu en C avec [raylib](https://www.raylib.com/). Le joueur arrive dans une zone de paix, puis monte une tour étage par étage. Chaque étage est un mini-jeu à gagner, et tous les 10 étages il y a un boss.

Les interfaces adaptent leur taille à la résolution de l'écran. Sigrid ne réagit au clic que sur les pixels visibles de son sprite.

Ce README explique comment les fichiers s'organisent et comment utiliser les fonctions principales.

## Sommaire

1. [Installation](#installation)
2. [Organisation du projet](#organisation-du-projet)
3. [Comment le jeu tourne (`main.c`)](#comment-le-jeu-tourne)
4. [Les fonctions, module par module](#les-fonctions-module-par-module)
5. [Ajouter un mini-jeu](#ajouter-un-mini-jeu)
6. [Règles de style du code](#règles-de-style-du-code)
7. [Problèmes fréquents](#problèmes-fréquents)

---

## Installation

### Installer une version prête à jouer

Chaque publication GitHub crée les paquets suivants :

- Windows x64 : `Hells-Tower-Setup-Windows-x64.exe`
- macOS Intel : `Hells-Tower-macos-intel.pkg` (assistant d'installation) ou `.dmg` (glisser-déposer)
- macOS Apple Silicon : `Hells-Tower-macos-apple-silicon.pkg` (assistant d'installation) ou `.dmg` (glisser-déposer)
- Linux x64 : `Hells-Tower-Linux-x64.AppImage` (rendre le fichier exécutable, puis le lancer)

Le `.pkg` macOS installe l'application dans `/Applications` et affiche les avis de licence des bibliothèques. Il n'est pas signé ; le `.dmg` reste disponible en alternative.

Au premier lancement de l'AppImage, Hell's Tower crée sans droits root son raccourci dans le menu des applications de l'utilisateur, conformément à FreeDesktop XDG.

Les paquets sont publiés dans **Releases**. Les builds de test sont aussi disponibles dans les artefacts de l'action **Build and package**. Les versions de test ne sont pas signées. Pour publier une version signée, créer un tag `vX.Y.Z` après avoir configuré les secrets de signature décrits ci-dessous.

La publication Windows nécessite `WINDOWS_SIGNING_PFX_BASE64` et `WINDOWS_SIGNING_PFX_PASSWORD` (certificat de signature de code au format PFX, encodé en Base64). La publication macOS nécessite `APPLE_CERTIFICATE_P12_BASE64`, `APPLE_CERTIFICATE_PASSWORD`, `APPLE_SIGNING_IDENTITY`, `APPLE_ID`, `APPLE_APP_SPECIFIC_PASSWORD` et `APPLE_TEAM_ID` (certificat Developer ID et compte Apple Developer). Les valeurs sont à enregistrer dans **Settings > Secrets and variables > Actions** du dépôt, jamais dans le code. Le `.dmg` macOS est signé et notarisé lorsque les secrets Apple sont configurés ; le `.pkg` est actuellement non signé. Linux AppImage n'a pas besoin de certificat.

### Compiler depuis les sources

Il faut CMake 3.16+, un compilateur C, Git et libcurl. Si raylib n'est pas déjà installée, CMake la télécharge et la compile à partir de la version 5.5.

Sur Linux (Ubuntu/Debian), installer d'abord les bibliothèques nécessaires :

```bash
sudo apt-get install build-essential cmake ninja-build git \
    libasound2-dev libcurl4-openssl-dev libgl1-mesa-dev libx11-dev libxext-dev libxinerama-dev \
    libxcursor-dev libxrandr-dev libxi-dev libxfixes-dev libxrender-dev \
    libwayland-dev libxkbcommon-dev
```

Sur Windows, utiliser MSYS2 avec GCC, Ninja et libcurl. Sur macOS, installer les outils en ligne de commande Xcode (`xcode-select --install`).

Puis compiler et lancer :

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Le binaire et les ressources sont dans `build/` ; `make` exécute ces mêmes étapes et met aussi à jour l'ancien exécutable `hells_tower` à la racine. Le fond animé est lu par raylib depuis `assets/menu_background.htv` à 24 images/s ; aucun lecteur vidéo externe n'est requis à l'exécution. Les paquets embarquent les ressources nécessaires, les avis de licence des bibliothèques et démarrent depuis leur répertoire d'installation.

Au clic sur **Jouer**, le jeu vérifie les nouvelles releases GitHub en arrière-plan. Si une version plus récente existe, l'écran de mise à jour apparaît à 75 % du chargement et bloque l'accès au jeu jusqu'à son installation. La version requise est mémorisée dans le dossier utilisateur avec la sauvegarde, afin que l'écran reste affiché même hors ligne; il disparaît au lancement d'une version au moins aussi récente. Les autres écrans de chargement durent environ 2,3 secondes.

#### Installer le build Linux dans son compte utilisateur

Après la compilation, installez le jeu sans `sudo` :

```bash
./packaging/linux/install.sh
```

Le script copie le jeu et ses ressources dans `~/.local/opt/hells-tower` et crée le raccourci **Hell's Tower** dans le menu des applications (`$XDG_DATA_HOME/applications`, ou `~/.local/share/applications` par défaut). Il suit le standard XDG et ne dépend pas d'un gestionnaire de paquets ; il fonctionne sur les distributions Linux avec un environnement de bureau compatible, dès lors que les dépendances de compilation du projet y sont installées. Relancez le script après une nouvelle compilation pour mettre à jour l'installation.

Les sauvegardes sont stockées dans le dossier utilisateur de chaque système : `%LOCALAPPDATA%/HellsTower` sous Windows, `~/Library/Application Support/Hells Tower` sous macOS, ou `$XDG_DATA_HOME/hells-tower` (par défaut `~/.local/share/hells-tower`) sous Linux. Une ancienne sauvegarde `save.txt` dans le répertoire du jeu est encore chargée et supprimée lors d'une réinitialisation.

---

## Organisation du projet

```
main.c                      point d'entrée
loading_screen/             écran de chargement avec barre de progression
updater.c / updater.h       vérification asynchrone des nouvelles releases
menu/
  initialisation.c          Init, LoadMenu, LoadBlackScreen, Shutdown
  update_screen.c           écran d'information lorsqu'une mise à jour est disponible
  background/               vidéo de fond du menu
  buttons/                  boutons Jouer / Options / Quitter
  title/                    titre animé
game/
  tower.c                   logique de la tour (étages, boss, sauvegarde)
  items/                    génération, affichage, sauvegarde et effets des items
  levels/                   niveaux, expérience et notification de niveau
  inventory/                interface de l'inventaire
  buttons/                  boutons de la zone de paix + entraînement
  chatbox/                  boîte de dialogue qui s'écrit lettre par lettre
  monster/                  monstre qui flotte dans la zone de paix
  pause_menu/               menu pause et fenêtre "session non sauvegardée"
  game_zone/
    zone.c                  cadre noir, monstre et dialogues autour du mini-jeu
    bullet hell/            mini-jeu 1
    rings/                  mini-jeu 2
    mirror/                 mini-jeu 3
assets/                     images, musiques, textes et médias
  bosses/                   portraits des boss
  easter_eggs/              images rares pour l'entraînement
  items/                    icônes des objets
  monsters/                 portraits des monstres
  musics/                   musiques du jeu
  items_effects.txt         effets des objets et mini-jeux concernés
```

### Assets présents

- À la racine : `button.png`, `button_move.mp3`, `button_click.mp3`, `cursor.png`,
  `dialoguebox1.png`, `heart.png`, `logo.png` (icône de l'application),
  `loading_screen.png`, `monster1.png`, `button_monster1.png`, `dead_sigrid.png`,
  `button_dead_sigrid.png`, `player.png`, `projectile.png`, `tower.png`,
  `menu_background.htv` (fond animé), `video.mp4` (source du fond),
  `monster_dialogues.txt`, `sigrid.txt`.
- `bosses/` : `Agarès, Démon de la Vengeance.png`,
  `Béhémot, Démon de l'Arrogance.png`, `Dame Marika, Celle qui Régna.png`,
  `Malénia, Lame Consort.png`, `Séléna, Déesse Déchue.png`.
- `easter_eggs/` : `easter_egg1.png`, `easter_egg2.png`.
- `items/` : `Anneau d'Ébène.png`, `Anneau du Pouvoir.png`, `Anneau Sombre.png`,
  `Bague de Mage.png`, `Bottes Usées.png`, `Boucle d'Oreille Sylvestre.png`,
  `Bouclier de Mage.png`, `Bouclier Magique Supérieur.png`, `Bouclier usé.png`,
  `Cape du Maraudeur.png`, `Cape du Voyageur.png`, `Coeur de Démon.png`,
  `Collier de Mage.png`, `Collier du Maudit.png`, `Coupe de Feu.png`,
  `Couronne Brisée.png`, `Couronne de Liche.png`,
  `Couronne du Souverain des Morts.png`, `Couronne Somnre.png`,
  `Crane de Créature.png`, `Crystal de Sang.png`, `Crystal Infusé.png`,
  `Ecusson de Mage.png`, `Faucille Étrange.png`, `Fiole de Poison.png`,
  `Fiole Étrange.png`, `Gantelet Magique.png`, `Gantelets Usés.png`,
  `Graine Démoniaque.png`, `Grimoire Démoniaque.png`, `Grimoire Maudit.png`,
  `Grimoire Noir.png`, `Lame d'Ébène.png`, `Lame Dragonicide.png`,
  `Lame du Héros.png`, `Lanterne Étrange.png`,
  `Pendentif de Chevalier de la Mort.png`, `Pendentif de la Liche.png`,
  `Pendentif du Premier Roi.png`, `Potion de Maléfice.png`,
  `Preuve de Victoire.png`, `Sablier Antique.png`, `Sablier Démoniaque.png`,
  `Sablier des Dragons.png`, `Sablier du Désert.png`, `Sceptre de la Liche.png`,
  `Sceptre de Liche.png`, `Souvenir du Passé.png`,
  `Symbole des Dragons.png`, `Tunique de Mage.png`.
- `monsters/` : `Le Premier Guide.png`, `Acolyte des Profondeurs.png`,
  `Arachnée, Reine des Araignées.png`, `Archevêque Sylvestre.png`,
  `Archidiacre Vane.png`, `Astor, l'Ombre.png`,
  `Aurelius, le Chevalier Sans Nom.png`, `Balthazar, le Héraut du Néant.png`,
  `Cardinal Thorne, l'Hérétique.png`, `Cassandre, la Prophétesse Maudite.png`,
  `Chevalier de la Mort.png`, `Chevalier Noir.png`, `Clerc Bestial Mort.png`,
  `Draven, le Seigneur des Ruines.png`, `Enfant de Fenrir.png`,
  `Enfant de la Veuve Noire.png`, `Erudit Solon, le Fou.png`,
  `Fragment du Géant.png`, `Isolde, la Veuve Noire.png`,
  `La Danseuse aux Pieds Nus.png`, `Leonard, Roi des Morts.png`,
  `Le Roi des Profondeurs.png`, `Le Roi sans Couronne.png`,
  `Malakor, l'Éclat Brisé.png`, `Mimic.png`,
  `Morgath, le Dévoreur de Serments.png`,
  `Ozymandias, l'Usurpateur Pâle.png`, `Plante Mutée.png`,
  `Serpent Hurleur.png`, `Sigrid, le Mage-Guide.png`,
  `Sorcière du Marais Gris.png`, `Souverain des Flammes.png`,
  `Thalès, Gardien du Seuil Éternel.png`,
  `Vespera, Dame de la Cendre Froide.png`, `Zarek, le Boucher Céleste.png`.
- `musics/` : `Soliloquy.mp3`, `ambiant.mp3`; dans `bosses/` :
  `boss1.mp3`, `boss2.mp3`, `boss3.mp3`, `boss4.mp3`, `the final battle.mp3`;
  dans `fights/` : `Boss Fight Bounce.mp3`, `bosstheme_WO_low.mp3`,
  `Komiku - It&#039;s time for adventure vol 2 - 13 Fight, run, breath deeply.mp3`.

---

## Comment le jeu tourne

```c
int main(void) {
  Init();

  while (!WindowShouldClose() && game_running) {
    LoadMenu();

    if (StartPlayingRequested()) {
      ClearStartPlayingRequest();
      LoadBlackScreen();
    }
  }

  Shutdown();
  CloseWindow();
  return 0;
}
```

| Fonction | Rôle |
|---|---|
| `Init()` | Ouvre la fenêtre en plein écran, charge les musiques, le curseur et la vidéo de fond. |
| `LoadMenu()` | Boucle du menu principal. Elle se termine quand le joueur clique sur **Jouer** ou **Quitter**. |
| `LoadBlackScreen()` | Boucle de la zone de paix (dialogue, boutons, tour, entraînement, pause). Elle se termine quand le joueur retourne au menu ou quitte. |
| `Shutdown()` | Libère toutes les textures, musiques et le son. |

`game_running` est une variable globale (définie dans `main.c`). Mets-la à `false` pour quitter le jeu proprement.

Chaque boucle suit le même schéma raylib :

```c
while (!WindowShouldClose() && game_running) {
  // 1. mises à jour (musique, écran de chargement, timers)
  BeginDrawing();
  ClearBackground(BLACK);
  // 2. dessin
  EndDrawing();
}
```

---

## Les fonctions, module par module

### Écran de chargement (`loading_screen.h`)

Affiche un fondu, une barre qui se remplit, puis exécute une fonction **à la fin du chargement**. C'est ce qu'on utilise pour changer d'écran avec une transition.

| Fonction | Rôle |
|---|---|
| `LoadingScreenInit()` | Charge l'image de fond par défaut. À appeler une fois au démarrage. |
| `LoadingScreenStart(action)` | Lance le chargement. `action` est une fonction `void f(void)` appelée quand la barre est pleine. |
| `LoadingScreenStartWithBackground(action, "chemin.png")` | Pareil, avec une autre image de fond. |
| `LoadingScreenUpdate()` | À appeler **chaque image**, avant `BeginDrawing()`. |
| `LoadingScreenDraw()` | À appeler **chaque image**, en dernier avant `EndDrawing()`. |
| `LoadingScreenActive()` | `true` tant que le chargement est en cours (sert à bloquer les clics). |
| `LoadingScreenUnload()` | Libère les textures. |

Exemple :

```c
static void AllerAuMenu(void) {
  menuDemande = true;
}

// quand le joueur clique sur un bouton :
LoadingScreenStart(AllerAuMenu);
```

Le chargement dure environ 6,6 secondes au total (fondu d'entrée, chargement, fondu de sortie). Les durées sont les `#define` en haut de `loading_screen.c`.

### Boîte de dialogue (`chatbox.h`)

Affiche un texte lettre par lettre. Un clic ou la touche **Espace** termine l'affichage du texte, puis passe au suivant.

| Fonction | Rôle |
|---|---|
| `chatbox(texte, "high")` ou `chatbox(texte, "low")` | Affiche la boîte en haut (`"high"`) ou en bas (`"low"`). À appeler chaque image. |
| `IsChatboxFinished(texte)` | `true` quand ce texte a été lu en entier et validé. |
| `IsChatboxInputConsumed()` | `true` si le clic dans la boîte ou la touche Espace a servi au dialogue. Un clic hors de la boîte reste disponible pour les boutons. |
| `ResetChatbox()` | Remet tous les dialogues à zéro. |
| `UnloadChatbox()` | Libère la texture. |

Pour enchaîner plusieurs dialogues, appelle `chatbox` pour **tous** les textes à chaque image. Seul le premier non terminé s'affiche :

```c
const char *dialogues[2] = {"Bonjour !", "Bienvenue a la Tour."};

for (int i = 0; i < 2; i++)
  chatbox(dialogues[i], "low");

if (IsChatboxFinished(dialogues[1])) {
  // tous les dialogues ont été lus
}
```

### Monstre de la zone de paix (`monster.h`)

| Fonction | Rôle |
|---|---|
| `monster(image, "middle", hoverEnabled)` | Dessine le personnage animé; si `hoverEnabled` vaut `true`, son image change au survol des pixels visibles et la fonction renvoie `true` au clic. |
| `UnloadMonster()` | Libère la texture. |

### Menu pause (`pause_menu.h`)

Les deux fonctions dessinent **et** renvoient le bouton cliqué.

```c
HomePauseResult r = DrawBlackScreenPauseMenu(&shadeAlpha);
if (r == HOME_PAUSE_RESUME)
  paused = false;
```

| Fonction | Retours possibles |
|---|---|
| `DrawBlackScreenPauseMenu(&shadeAlpha)` | `HOME_PAUSE_NONE`, `HOME_PAUSE_RESUME`, `HOME_PAUSE_RETURN_TO_MENU`, `HOME_PAUSE_QUIT` |
| `DrawUnsavedSessionDialog()` | `UNSAVED_DIALOG_NONE`, `UNSAVED_DIALOG_CONTINUE`, `UNSAVED_DIALOG_CANCEL` |

`shadeAlpha` est un `float` à toi, qu'il faut remettre à `0` à chaque ouverture de la pause (c'est lui qui fait le fondu au noir).

### Zone de jeu (`zone.h`)

Dessine le cadre noir où se joue le mini-jeu, le monstre au-dessus, et ses phrases quand le joueur est touché.

```c
DrawGameZone(BulletHellUpdate, BulletHellDamageCount, "assets/monsters/gobelin.png");
```

| Paramètre | Rôle |
|---|---|
| 1er : fonction de mise à jour du mini-jeu | Elle reçoit le `Rectangle` de la zone et dessine le mini-jeu dedans. |
| 2e : fonction qui renvoie le nombre de coups reçus | Sert à déclencher une phrase du monstre quand ce nombre augmente. Peut être `NULL`. |
| 3e : chemin de l'image du monstre | Peut être `NULL` ou `""` pour ne pas afficher de monstre. |

`UnloadGameZone()` libère les textures.

### Inventaire (`inventory.h`)

| Fonction | Rôle |
|---|---|
| `DrawInventory(allowClose)` | Dessine l'inventaire au-dessus du menu et renvoie `false` à Échap ou après un clic hors du panneau. |
| `UnloadInventory()` | Libère le portrait chargé par l'inventaire. |

Le niveau et l'expérience affichés viennent du module `levels/`. Chaque niveau
gagné attribue un item si la zone de stockage n'est pas pleine. Les items sont
affichés sous les cinq emplacements équipés; un clic droit les déplace vers le
premier emplacement libre de l'autre zone avec le clic gauche. Le clic droit
ouvre ses effets et le bouton **Jeter l'artefact** le détruit. Les items équipés
ne sont plus consommés après une partie; leurs effets compatibles sont réappliqués
au prochain lancement sans cumuler plusieurs fois le même effet. Les changements
d'inventaire sont conservés dans la sauvegarde en cliquant sur **Sauvegarder**.
Chaque effet généré tire sa rareté séparément : **Commun 50 %**, **Rare 30 %**,
**Épique 15 %**, **Légendaire 4 %** et **Maudit 1 %**. Un effet déjà présent sur
l'item ne peut pas être tiré une deuxième fois.
Le portrait
`assets/player0.png` est affiché avec le nom « Melina ».

### Les mini-jeux

Les mini-jeux de la tour ont **exactement la même interface** (remplace `Xxx` par `BulletHell`, `Rings`, `Mirror` ou `Seals`) :

| Fonction | Rôle |
|---|---|
| `XxxStart(difficulte)` | Lance une partie. `difficulte` va de 1 à 10. |
| `XxxUpdate(zone)` | Met à jour **et** dessine le mini-jeu. À donner à `DrawGameZone`. |
| `XxxState()` | Renvoie l'état de la partie (voir ci-dessous). |
| `XxxDamageCount()` | Nombre de coups reçus depuis le début. |
| `XxxUnload()` | Libère les textures. |

États possibles :

| Mini-jeu | Jouer | Gagné | Perdu | Terminé |
|---|---|---|---|---|
| Bullet hell | `HELL_PLAYING` | `HELL_WON` | `HELL_LOST` | `HELL_OFF` |
| Rings | `RINGS_PLAYING` | `RINGS_WON` | `RINGS_LOST` | `RINGS_OFF` |
| Mirror | `MIRROR_PLAYING` | `MIRROR_WON` | `MIRROR_LOST` | `MIRROR_OFF` |
| Seals | `SEALS_PLAYING` | `SEALS_WON` | `SEALS_LOST` | `SEALS_OFF` |

Le joueur a 3 vies. Bullet hell, Rings et Mirror durent 30 secondes ; Seals dure 50 secondes et demande d'activer quatre sceaux. Dans la tour, un menu s'affiche après une victoire ou une défaite pour continuer, recommencer ou sortir.

| Mini-jeu | Principe |
|---|---|
| **Bullet hell** | Des projectiles viennent de partout et suivent le cœur. Déplacement à la souris ou aux touches WASD / flèches. |
| **Rings** | Des anneaux se rétrécissent vers le centre, avec une ouverture à trouver. Déplacement à la souris. |
| **Mirror** | L'écran est coupé en deux : le cœur est dupliqué des deux côtés (en miroir) et les projectiles visent chaque copie. |
| **Seals** | Réservé aux boss : esquive les attaques annoncées, puis reste sur chaque sceau pour en activer quatre. Déplacement avec WASD / flèches, esquive avec **Espace**. |

Les attaques et les vagues montent progressivement avec la difficulté; le
combat d'entraînement permet de sélectionner un jeu et lance une difficulté
aléatoire de 8 à 10. Les easter eggs y apparaissent à 1 % lorsqu'il existe des
monstres ordinaires à afficher.

### La tour (`tower.h`)

| Fonction | Rôle |
|---|---|
| `Tower()` | À appeler chaque image quand le joueur est dans la tour. Affiche les choix de recommencer/sortir après une défaite et d'aller à l'étage suivant/sortir après une victoire. |
| `UpdateTowerMusic()` | À appeler chaque image pour que la musique continue de jouer. |
| `LoadTowerProgress()` | Lit la sauvegarde dans le dossier utilisateur (ou l'ancien `save.txt` local). Si elle est absente ou invalide, on repart de l'étage 1. |
| `SaveTowerProgress()` | Écrit la sauvegarde dans le dossier utilisateur. Renvoie `true` si ça a marché. |
| `ResetTowerProgress()` | Remet étage, niveau, expérience et dialogues à zéro en mémoire. |
| `TowerProgressNeedsSaving()` | `true` si la partie a changé depuis la dernière sauvegarde. |
| `TowerTakeExitRequest()` | Indique que le joueur a choisi de sortir de la tour. |
| `TowerTutorialSeen()` / `MarkTowerTutorialSeen()` | Savoir si le joueur a déjà vu les dialogues d'introduction, et le noter. |
| `ResetTower()` | Arrête la musique et libère tous les mini-jeux. |

Règles de la tour :

- un boss tous les 10 étages (10, 20, 30, 40, 50) : mini-jeu Seals, difficulté 7 à 9 (le dernier boss : difficulté 10)
- après la victoire à l'étage 50, l'étage suivant est le 51
- les étages finissant par 5 (5, 15, 25, 35, 45) : difficulté 7
- les autres étages : difficulté 1 à 6, mini-jeu choisi au hasard (les répétitions sont possibles)
- l'adversaire et la musique viennent de `assets/monsters`, `assets/musics/fights` (ou de la liste `bosses` pour les boss)
- chaque seconde jouée rapporte étage × difficulté XP, même en cas de défaite
- une victoire normale multiplie le gain par 3; une victoire contre un boss le multiplie par le numéro de l'étage (par exemple : 30 × 10 × 8 × 10 XP au boss de l'étage 10)
- les gains et la progression restent en mémoire jusqu'au clic sur **Sauvegarder**
- la sauvegarde est chargée pendant l'écran de chargement lancé par **Jouer** au menu principal; les autres écrans de chargement ne la relisent pas
- le joueur commence au niveau 0 avec 0 XP; le niveau suivant demande 100 XP, puis le seuil augmente de 1,5 fois par niveau

Format du fichier `save.txt` dans le dossier de données utilisateur :

```
<étage> <tuto_vu> <difficulté> <niveau> <xp> <masque_dialogues_sigrid> 2
<chemin de l'image du monstre>
<chemin de la musique>
<mini-jeu : bullethell, rings, mirror, ou vide pour un boss>
<items équipés et stockés (ligne facultative pour les anciennes sauvegardes)>
```

### Dialogues de Sigrid (`assets/sigrid.txt`)

Chaque ligne décrit un étage. Quand le joueur clique sur Sigrid dans la zone de
paix à cet étage, les dialogues s'affichent dans l'ordre. Un clic ou **Espace**
révèle le texte complet, puis passe au suivant. Après la première lecture, les
clics suivants affichent uniquement `PHRASE_FIN`. Les étages déjà lus sont
conservés dans le fichier de sauvegarde utilisateur. Les dialogues ne remplacent pas le tutoriel initial.

Chaque ligne du fichier suit ce format :

```
NB_DE_DIALOGUES;DIALOGUE1&DIALOGUE2&...;ETAGE;PHRASE_FIN
```

`NB_DE_DIALOGUES` compte aussi `PHRASE_FIN`; il faut donc une entrée de moins
dans la liste des `DIALOGUE`. `&` et `;` servent de séparateurs et ne peuvent
pas apparaître dans les textes. Les étages lus sont gardés en mémoire et ne
sont écrits dans le fichier de sauvegarde utilisateur qu'en cliquant sur **Sauvegarder**. À partir de
l'étage 51, **Monter la tour**
devient **Altérer le destin** : le jeu passe par l'écran de chargement, remet
la progression à zéro et revient au menu principal.

### Niveaux et expérience (`levels/levels.h`)

| Fonction | Rôle |
|---|---|
| `LevelsLoad(niveau, xp)` | Charge la progression sauvegardée. |
| `LevelsLevel()` / `LevelsXP()` | Renvoient le niveau actuel et l'XP accumulée vers le suivant. |
| `LevelsXPNeeded()` | Renvoie l'XP nécessaire pour le prochain niveau : `ceil(100 × 1,5^niveau)`. |
| `LevelsAddXP(montant)` | Ajoute de l'XP et applique les niveaux gagnés. |
| `LevelsDrawNotification()` | Affiche le gain d'XP avec une barre animée pendant 5 secondes; une montée de niveau s'affiche d'abord en grand. |

### Boutons de la zone de paix (`game/buttons/buttons.h`)

| Fonction | Rôle |
|---|---|
| `DrawGameButtons(acceptInput)` | Affiche les 6 boutons (Sauvegarder, Menu principal, Quitter, Combat d'entrainement, Monter la tour, Inventaire). Le combat d'entraînement propose les quatre mini-jeux et une difficulté réglable de 1 à 10. |
| `UpdateGameButtonsMusic()` | Met à jour la musique en cours. |
| `GameSubscreenActive()` | `true` si le joueur est dans la tour. |
| `GameTrainingActive()` | `true` si le joueur est en combat d'entraînement. |
| `GameReturnToMenuRequested()` | `true` une seule fois quand le joueur a demandé à revenir au menu principal. |
| `ResetGameButtons()` | Remet tous les états à zéro. |
| `UnloadGameButtons()` | Libère les textures et la police. |

### Menu principal

| Fonction | Rôle |
|---|---|
| `Button("Jouer")` / `Button("Options")` / `Button("Quitter")` | Dessine le bouton et exécute son action au clic. Seuls ces trois noms sont reconnus. |
| `StartPlayingRequested()` | `true` quand "Jouer" a été cliqué. |
| `ClearStartPlayingRequest()` | Remet cette demande à `false`. |
| `DrawMenuTitle("Hell's Tower")` | Dessine le titre animé. |
| `InitMenuBackground()` | Charge le GIF animé du menu avec raylib. |
| `UpdateAndDrawMenuBackground()` | Avance et affiche l'image suivante du GIF, en boucle. |
| `UnloadMenuBackground()` | Libère les images et la texture du fond. |

---

## Ajouter un mini-jeu

1. Copie un mini-jeu existant (par exemple `rings/`) et renomme les fonctions : `MonJeuStart`, `MonJeuUpdate`, `MonJeuState`, `MonJeuDamageCount`, `MonJeuUnload`.
2. Dans `tower.c`, ajoute son nom dans `gameNames` (utilisé pour la sauvegarde), et passe le nombre de jeux de 3 à 4 dans `GameIndex` et `PickMinigame`.
3. Ajoute un `case` dans les `switch` de `StartGame`, `UpdateFloorGame`, `FloorGameState` et `FloorGameDamageCount`.
4. Appelle `MonJeuUnload()` dans `ResetTower`.

---

## Règles de style du code

Le code est écrit pour rester lisible quand on débute en C :

- une variable par ligne
- pas de ternaire `? :`, on utilise un `if`
- pas de `raymath.h` : les calculs sont écrits avec `x` et `y`
- `Limit(valeur, min, max)` pour garder une valeur entre deux bornes
- les fonctions longues sont découpées (`MovePlayer`, `UpdateBullet`, etc.)
- les variables `static` en haut d'un fichier sont **privées** à ce fichier : les autres fichiers n'y ont accès que par les fonctions publiques du `.h`

---

## Problèmes fréquents

| Problème | Solution |
|---|---|
| Écran noir, textures manquantes | Vérifie que le dossier `assets/` est présent à côté du jeu (ou dans les ressources de l'application macOS). |
| Le fond animé du menu ne s'affiche pas | Vérifie que `assets/menu_background.htv` est présent ; le menu utilise une image statique de secours. |
| `NULL is undefined` | Ajoute `#include <stddef.h>`. |
| `snprintf` inconnu | Ajoute `#include <stdio.h>`. |
| `game_running` inconnu | Ajoute `extern bool game_running;` sous les `#include`. |
| Le jeu repart de l'étage 1 | La sauvegarde utilisateur est absente ou invalide : sauvegarde depuis le bouton **Sauvegarder**. |
