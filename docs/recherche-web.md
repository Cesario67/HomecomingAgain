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
