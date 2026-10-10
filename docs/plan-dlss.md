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
| Le jeu est en D3D9 | Vérifié (imports de g_SilentHill.sgl) | Passer le jeu en D3D9Ex (même interface, ajoute le partage de surfaces) |
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
2. **Passage en D3D9Ex et profondeur INTZ** : vérifier que le jeu tourne identique, puis afficher la profondeur
   en surimpression (touche de debug).
3. **Vecteurs de mouvement** : shader de reconstruction, affichage en couleurs pour contrôle.
4. **Processus auxiliaire et partage** : sans DLSS d'abord (il recopie l'image telle quelle), pour mesurer le
   coût et valider la synchronisation, Alt-Tab, `Reset` et l'overlay Steam.
5. **DLAA** : NGX en mode DLAA sans changer la résolution de rendu, jitter actif. Essai décisif sur les traînées.
6. **DLSS** : rendu interne réduit (Qualité 67 %, Équilibré 58 %, Performance 50 %), ce qui demande de réduire la
   taille des cibles de rendu de la scène sans toucher à celles du HUD.
7. **Launcher** : options DLSS (Désactivé / DLAA / Qualité / Équilibré / Performance), détection de la carte
   RTX et de la DLL, réglage de netteté.

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
