# Recherches sur Internet (09/10/2026)

Ce que disent les sources publiques sur Silent Hill: Homecoming PC (Steam, appid 19000). **Rien de ceci n'est
vérifié sur l'exécutable** : ce sont des témoignages, souvent anciens (2015 à 2022). À confirmer par
désassemblage dès que le jeu est installé (voir `docs/a-faire.md`).

## Correctif communautaire existant : Silent Hill Homecoming PC Fix (Steam006, 2018)

Source : [DSOGaming](https://www.dsogaming.com/news/silent-hill-homecoming-mod-enables-60fps-adds-controllable-fov-patches-hard-coded-720p-resolution),
fichier hébergé dans la section communautaire de PCGamingWiki (id 1144).

- Débloque le 60 i/s, ajoute un champ de vision réglable (Page Up / Page Down).
- Corrige des valeurs de résolution 720p codées en dur, dont la carte et les objets.
- MSAA jusqu'à 8x, V-Sync forcé en plein écran (mode fenêtré recommandé).
- Active le drapeau « 4 Go » (LARGE_ADDRESS_AWARE) et le DEP sur `SilentHill.exe`.
- Corrige « un plantage courant » (non détaillé).
- Fonctions de débogage : brouillard DirectX (F1), fil de fer (F2), traverser les murs (F3), taille de la carte
  et des objets (F4/F5).
- Se lance par son propre exécutable (`Silent_Hill_Homecoming_PC_Fix.exe`) depuis le dossier `Bin` du jeu.
- Ne traite pas la manette. N'est pas notre code : on s'en sert comme **liste de ce qui est possible**, on
  ne copie ni son binaire ni son code.

## Faits relevés (témoignages Steam, à vérifier)

- L'exécutable est `SilentHill.exe` dans `Bin\`. Le jeu est en Direct3D 9.
- Cadence codée à 30 i/s selon plusieurs témoignages, même si l'on change le réglage.
- Plantage au-delà de 720p et/ou 60 Hz (2018) ; contournement par un wrapper D3D9 dans `Bin`.
- Plantage au lancement sur Windows 10/11 : une édition du fichier `vars_pc` (dossier `Engine`) est signalée comme
  remède (2022). Le contenu exact reste à retrouver et à comprendre, pas à recopier tel quel.
- Fichiers de configuration dans `Engine\` : `vars_pc` (réglages) et `binds_pc_mjs.cfg` (touches). Les réglages
  faits en jeu seraient réinitialisés à la fermeture.
- Format 21:9 : écran noir ou carte tronquée signalés.
- Windows 11 : écran noir ou résolution trop grande, soulagés par le remplacement de la mise à l'échelle
  DPI hautes dans les propriétés de compatibilité de l'exécutable.
- Manette : le jeu propose un réglage XInput dans ses options ; des boutons cessent de répondre après le
  chargement d'une sauvegarde ; certains joueurs ne la font fonctionner qu'avec un émulateur de manette.

## Pistes d'analyse qui en découlent

1. Importer la table d'imports : `d3d9.dll` statique ? `xinput`/`dinput8` ? `Sleep` ? (détermine le chargeur et
   les points d'interception, comme `X3DAudio1_7.dll` pour Deadly Premonition).
2. Trouver le verrou 30 i/s (constante, `Sleep`, boucle de temps) et la dépendance de la logique à la cadence.
3. Trouver les constantes 720p et les tailles de cibles de rendu (carte, objets).
4. Trouver la lecture des manettes et la table de boutons (`binds_pc_mjs.cfg`).
5. Chercher la cause du plantage sur machines récentes (minidump du filtre d'exceptions, pas de supposition).

Sources : discussions Steam du jeu (appid 19000), DSOGaming, PCGamingWiki (page principale inaccessible en 403 au
moment de la recherche).

# Projets existants (recherche complémentaire du 09/10/2026)

## 1. unknownproject/Silent_Hill_Homecoming (le projet de référence)

Dépôt [github.com/unknownproject/Silent_Hill_Homecoming](https://github.com/unknownproject/Silent_Hill_Homecoming)
(196 commits, dernier push le 24/02/2026), prolongement du correctif de Steam006 par un auteur seul, actif depuis
01/2018. Versions 1.x (2018-2019), 2.x (2019-2021), 2.5 (12/2021), 3.0 d à h (2022-2023), **3.10 AIO (08/08/2024)**.
Journal des modifications : `Patches/ChangeLog.txt` du dépôt. Distribué sous forme d'archives et d'installeurs
binaires, **sans code source** ("About 10000 lines" d'assembleur selon l'auteur) et **sans licence**. L'auteur
écrit explicitement que son code est à lui et que personne ne l'utilisera sans son accord.

Ce qu'il fait déjà (d'après son journal, non vérifié par nous) :
- **Cadence** : limiteur de 30 à 300 i/s réécrit cinq fois, dialogues synchronisés sur la cadence, animations et
  physique corrigées au-delà de 60 i/s ("twitching" physics, personnages qui parlent trop vite), isolation des
  QTE, du grain et des clignotements vis-à-vis de la cadence.
- **Mémoire et stabilité** : drapeau 4 Go sur l'exécutable ET sur `g_SilentHill.sgl`, gestion mémoire et
  multithreading améliorés, plantages d'écran de chargement corrigés, interblocages éliminés, sauvegarde
  automatique d'origine désactivée (elle gelait le jeu) puis points de contrôle restaurés (2.0 upd 3), dossier de
  sauvegarde déplacé dans `Engine\savegame`.
- **Alt-Tab** : l'algorithme de "périphérique perdu" **ferme silencieusement le jeu** pour éviter le plantage
  (v2.x) ; la v3.x s'appuie sur le modèle de pilote WDDM pour supprimer le problème (Windows Vista à 10).
- **Graphismes** : résolutions jusqu'à 4320x2160 (interface, carte, objets, photos), 21:9 expérimental, V-Sync et
  anticrénelage natifs corrigés, filtre de tonalité rétabli, gamma, effets spéciaux et caméra réglables.
- **Manette et interface** : icônes de boutons manquantes restaurées (PS3 / Xbox 360 / clavier-souris), commutateur
  d'invites de boutons, QTE "amicaux" (facile / automatique), sensibilité de la souris augmentée.
- **Divers** : mode debug (touches du pavé numérique, mode gel, redémarrage du niveau), difficulté rééquilibrée et
  objectifs désactivés ("HARD MODE"), langues polonais, tchèque, russe, gestionnaire de ressources pour un
  `global.pak` personnalisé.
- **Protections** : "anti-tampering / anti-injection / anti-wrappers" (annoncées dans la v2.x). Conséquence
  possible : un wrapper D3D9 ou un chargeur DLL tiers peut être refusé par SON patch. Notre mod doit donc être
  utilisé SANS son patch (jeu d'origine), jamais empilé dessus.

Limites rapportées par des joueurs (témoignages, à vérifier) : toujours 24 à 30 i/s selon l'ordre d'installation
(2.5 avant 3.0), la sensibilité de la caméra à la manette trop élevée (Steam Deck inclus), les QTE pas tous
remplacés, les touches clavier câblées en dur, l'expérience qui s'éloigne du jeu d'origine (difficulté, objectifs),
mot de passe puis Discord pour obtenir la dernière version, installation en archive (écrase exécutable et fichiers).

## 2. Autres éléments

- **Steam006 PC Fix (2018)** : DLL + INI + exécutable de lancement, fichier communautaire PCGamingWiki. Ancêtre du
  précédent. A aussi un remplaçant du fichier `g_SilentHill.sgl`.
- **Guide Steam "make Silent Hill Homecoming playable + Xbox 360 controller"** (2022, id 2912932040) : s'appuie sur
  le patch d'unknownproject et sur un **émulateur de manette Xbox 360** déposé dans `Bin`, preuve que la prise en
  charge native de la manette (XInput) reste un manque.
- **Special K** : contournement de fréquence recommandé par PCGamingWiki.
- **Patcher de grasmann** : retire le grain des écrans de chargement et remplace les icônes de boutons (sauvegarde
  dans le registre).
- **ENB** : préréglage d'éclairage et de post-traitement sur Nexus Mods (pas une restauration de textures).
- Aucun projet de restauration de textures HD trouvé.

## Ce que cela change pour HomecomingAgain

Constat honnête : 60 i/s, 4 Go, résolutions élevées, 21:9, QTE et invites de boutons sont **déjà couverts** par un
patch très abouti, mais fermé, sans licence et qui remplace des fichiers du jeu. Créneaux où HomecomingAgain peut être
utile (à valider par l'analyse de l'exécutable) :
1. **Ouvert et auditable** (GPL, code lisible) alors que l'existant est un binaire opaque.
2. **Non destructif** : exécutable et `.sgl` d'origine inchangés, un chargeur DLL, désinstallation propre.
3. **Alt-Tab / périphérique perdu** traité (`Reset`) au lieu de fermer le jeu.
4. **Manette XInput native** (sans émulateur) avec sensibilité de caméra réglable, et clavier-souris rebindable.
5. **Diagnostic** : journaux et minidumps pour documenter les plantages restants (méthode DPStabilityFix).
6. **Compatibilité d'environnement** : Windows 11, GPU récents, Steam Deck / Proton.
Sinon, l'utilisateur peut simplement installer le patch 3.10 : à discuter avant d'investir (voir docs/a-faire.md).

## Règles qui en découlent (reportées dans CLAUDE.md)

- Ne jamais télécharger ni désassembler le patch d'unknownproject ou de Steam006 pour en reprendre le code ou les
  adresses : sans licence et avec une opposition explicite de l'auteur. Nos adresses viennent uniquement de notre
  analyse de l'exécutable d'origine.
- Tester sur un jeu d'origine, jamais avec leur patch installé.
