# Plan : DLSS dans Silent Hill: Homecoming

État : **recherche terminée, rien n'est codé**. Ce document dit ce que le DLSS exige, ce que le jeu fournit,
les obstacles identifiés et l'ordre des travaux. Tout point marqué *hypothèse* reste à vérifier.

## 1. Ce que le DLSS exige (sources NVIDIA, vérifié)

- Une API récente : D3D11, D3D12 ou Vulkan (SDK NGX ou Streamline).
- Par image : la couleur à la résolution de rendu, la profondeur, des vecteurs de mouvement (en pixels), le
  décalage de caméra (« jitter ») appliqué à cette image, et une texture de sortie à la résolution finale.
- Une carte RTX : la machine de test a une RTX 5070 Ti (pilote 32.0.16.1714), plus un GPU AMD intégré.

## 2. Obstacles identifiés

| Obstacle | Constat | Réponse prévue |
|---|---|---|
| Le jeu est en D3D9 | Vérifié (imports de g_SilentHill.sgl) | Exécuter le jeu par **D3D9On12** (couche de Windows, présente en 32 bits) : ressources Direct3D 12, partageables entre processus avec des fences. D3D9Ex, envisagé d'abord, refuse D3DPOOL_MANAGED |
| Le jeu est 32 bits, le DLSS 64 bits seulement | Les SDK ne livrent que `Windows_x86_64` (*hypothèse forte* : aucune version 32 bits trouvée) | Un **processus auxiliaire 64 bits** (`HomecomingAgain.Dlss.exe`) fait le DLSS en D3D11 sur des textures partagées |
| Partage D3D9Ex vers D3D11 | Documenté : texture 2D, 1 niveau, format A8R8G8B8 / A16B16G16R16F / A2B10G10R10, créée par D3D9Ex avec `pSharedHandle`, ouverte par `OpenSharedResource`. Pas de keyed mutex avec D3D9Ex | Textures créées côté jeu, synchronisation par requête `D3DQUERYTYPE_EVENT` + événements Win32 |
| Deux GPU dans la machine | Le partage n'est possible que sur le même adaptateur | Le processus auxiliaire choisit l'adaptateur par LUID, transmis par le mod |
| Profondeur | Le moteur crée son tampon de profondeur lui-même | Remplacer, dans `CreateDepthStencilSurface` / `CreateTexture`, le format par **INTZ** (méthode de ReShade) pour pouvoir le lire |
| Vecteurs de mouvement | Le moteur n'en produit pas | Les reconstruire par shader : profondeur + matrices de caméra de l'image courante et de la précédente |
| Matrices de caméra | Envoyées aux shaders par `SetVertexShaderConstantF`, registres inconnus | À identifier par le relevé d'image (étape 1) |
| Jitter | Absent | Ajouter un décalage sous-pixel à la matrice de projection au moment où elle est envoyée |
| Interface (HUD, menus) | Dessinée par-dessus la scène, moment inconnu | Repérer le premier appel de l'interface et lancer le DLSS juste avant |
| Objets animés, brouillard, particules | Les vecteurs reconstruits n'ont que le mouvement de caméra | Risque de traînées. Mesuré au prototype, masques de réactivité si besoin |
| Licence | DLL NVIDIA non redistribuable sans conditions claires, projet GPL | DLL **non fournie** : le launcher la cherche (pilote, jeu Steam du joueur) ou demande au joueur de la placer |

## 3. Architecture visée

```
Jeu 32 bits (D3D9Ex)                      HomecomingAgain.Dlss.exe (64 bits, D3D11)
 version.dll (mod)                          
  - profondeur INTZ                         
  - jitter dans la projection               
  - à la fin de la scène, avant le HUD :    
      copie couleur + profondeur  ── textures partagées ──>  ouvre les textures (OpenSharedResource)
      calcul des vecteurs (shader)                           NGX DLSS (DLAA puis Qualité/Performance)
      signal « image prête »      ── événement ──────────>   évaluation
      attente « résultat prêt »  <── événement ───────────   écrit la texture de sortie partagée
      copie la sortie dans le backbuffer, puis le jeu dessine le HUD et Present
```

- Le processus auxiliaire est lancé par le mod, meurt avec le jeu (job object), et tout échec (pas de RTX,
  DLL absente, délai dépassé) ramène au SMAA actuel sans planter.
- Coût attendu : un aller-retour entre processus par image, à mesurer (cible : moins de 1 ms).

## 4. Étapes

Chaque étape se termine par un essai en jeu. On s'arrête dès qu'une étape montre une impasse.

1. **Relevé d'image** (`[Debug] FrameCapture`) : pour une image déclenchée par une touche, journaliser chaque
   `SetRenderTarget`, `SetDepthStencilSurface`, `Clear`, création de surface, `DrawIndexedPrimitive` (nombre
   seulement) et chaque `SetVertexShaderConstantF` (registres et valeurs). Objectif : trouver le tampon de
   profondeur, les registres de la vue et de la projection, la fin de la scène 3D et le début du HUD.
2. **Passage par D3D9On12** ([Graphics] D3D9On12, expérimental) : vérifier que le jeu tourne identique (image,
   cadence, overlay Steam, SMAA, SSAO). La lecture de la profondeur est déjà validée par le SSAO (10/10/2026).
3. **Vecteurs de mouvement** : shader de reconstruction, affichage en couleurs pour contrôle.
4. **Processus auxiliaire et partage** : sans DLSS d'abord (il recopie l'image telle quelle), pour mesurer le
   coût et valider la synchronisation, Alt-Tab, `Reset` et l'overlay Steam.
5. **DLAA** : NGX en mode DLAA sans changer la résolution de rendu, jitter actif. Essai décisif sur les traînées.
6. **DLSS** : rendu interne réduit (Qualité 67 %, Équilibré 58 %, Performance 50 %), ce qui demande de réduire la
   taille des cibles de rendu de la scène sans toucher à celles du HUD.
7. **Launcher** : options DLSS (Désactivé / DLAA / Qualité / Équilibré / Performance), détection de la carte
   RTX et de la DLL, réglage de netteté.

## 4 bis. Résultats du relevé (10/10/2026, prologue, vérifié par relevé)

Ordre d'une image (environ 10 000 appels, 800 dessins) :

1. Trois cartes d'ombres 2048x2048 (R32F, profondeur D24S8 dédiée).
2. **Pré-passe de profondeur** dans une texture 1920x1080 **R32F** (texture 0x79bc7e0 au relevé, effacée à 1.0),
   avec le tampon D24S8 1920x1080 principal. La profondeur est donc déjà lisible : INTZ inutile (à confirmer en
   affichant cette texture).
3. Scène principale dans une texture 1920x1080 A8R8G8B8 (0x7a4d8e0), même tampon de profondeur.
4. Post-traitements : cibles A16B16G16R16F 1920x1080 sans profondeur, puis chaîne de flou lumineux (960x540
   jusqu'à 15x8), recopiée par StretchRect.
5. **Interface** : dernière scène (BeginScene ... EndScene) sur la même texture couleur, avec profondeur.
6. StretchRect de la texture couleur vers le backbuffer, puis Present.

Shaders : la table de constantes est présente, les noms sont lisibles. `mWorldViewProj` est toujours en c0..c3
des vertex shaders (24 shaders), `mWorld` en c4..c6, `mSkinningMatrices` en c14..c193 pour les personnages,
`mWorldView` et `mProj` dans quelques shaders seulement.

Conséquences pour le plan :
- **Jitter** : il peut s'appliquer directement à `mWorldViewProj` à chaque envoi (décalage ajouté en espace de
  découpage), sans isoler la projection.
- **Vecteurs de mouvement de caméra** : la vue-projection se déduit de `mWorldViewProj` et `mWorld` quand un shader
  a les deux.
- **Personnages** : les matrices d'os passent par `mSkinningMatrices`, une piste pour de vrais vecteurs de
  mouvement à l'étape 4.
- **Point d'insertion du DLSS** : juste avant les post-traitements (fin de l'étape 3), l'interface venant après.

## 4 ter. Essais D3D9On12 (10/10/2026, vérifiés)

- Le rendu fonctionne : l'image du tampon arrière, enregistrée juste avant Present (`[Debug] BackBufferDump`), est
  correcte (menu complet). Present ne renvoie aucune erreur. Mais la fenêtre reste noire.
- Éliminé : nos effets (SMAA, SSAO), le mode sans bordure, la fenêtre de présentation (hDeviceWindow et fenêtre
  forcée bien renseignés), le mode COPY (DISCARD essayé), le choix de la carte (périphérique Direct3D 12 créé sur la
  RTX), l'overlay Steam (jeu lancé sans), toutes les fonctions du mod, la rampe gamma (normale, ignorée sans effet).
- Un programme de test minimal (fenêtre, effacement rouge, Present) affiche correctement par D3D9On12 sur la même
  machine, y compris en COPY, multithread, Present depuis un autre thread et avec fenêtre forcée.
- Conclusion : incompatibilité entre ce moteur et D3D9On12, cause non trouvée. Cadence aussi divisée par deux
  (31 i/s au menu au lieu de 60).
## 4 quater. Voie DXVK (13/10/2026)

- Jalon 1 (vérifié) : sous DXVK (`d3d9.dll` de DXVK dans `Bin`), le mod obtient le périphérique Vulkan et les images
  de la scène et de la profondeur par l'interface d'interopérabilité de DXVK.
- Jalon 2 (vérifié en jeu) : `[DLSS] ShareFrames=1`, la scène (B8G8R8A8) et la profondeur (R32F) sont copiées chaque
  image dans des images Vulkan exportées (handles NT) avec un sémaphore timeline exporté. 592 images sur 592 en 10 s,
  0,16 ms de processeur par image, 59 i/s inchangés.
- Jalon 3 (codé, pas encore testé) : `HomecomingAgain.Dlss.exe` (64 bits, `helper/`), lancé par le mod avec les trois
  handles hérités, importe les images en Vulkan, attend le compteur et enregistre une image de contrôle de la scène
  et de la profondeur dans `HomecomingAgain\captures`. Journal : `HomecomingAgain\logs\HomecomingAgain.Dlss.log`.
- Limite connue : le jeu réécrit les images partagées à l'image suivante sans attendre le lecteur (pas encore de
  synchronisation en retour). À régler avant le DLSS (anneau d'images ou sémaphore en retour).
## 5. Plans de repli

- Si les traînées sont inacceptables et que les animations ne peuvent pas être suivies : garder DLAA sur les
  scènes statiques n'a pas d'intérêt, on revient au **rendu en haute définition** (supersampling interne ou
  DLDSR), qui ne demande aucune de ces étapes.
- Les outils existants (DLSS5-Feeder avec dgVoodoo2, renodx) sont des chaînes externes : on ne les intègre pas
  (licences, dépendance à dgVoodoo2 qui remplacerait notre propre hook D3D9).

## Sources

- NVIDIA Streamline, guide DLSS : https://github.com/NVIDIAGameWorks/Streamline/blob/main/docs/ProgrammingGuideDLSS.md
- Partage D3D9Ex / D3D11 : https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-opensharedresource
- Profondeur INTZ en D3D9 (ReShade) : https://reshade.me/forum/general-discussion/4083-depth-buffer-detection-modification
- DLSS pour jeux sans DLSS, approche comparable : https://github.com/Aryoksini/DLSS5-Feeder
- Licence des DLL DLSS : https://forums.developer.nvidia.com/t/updating-unity-dlss-nvngx-dlss-dll-file-licensing/329558
