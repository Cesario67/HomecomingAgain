# SMAA (Jimenez et al.) : origine des fichiers

- `SMAA.h` (shader HLSL), `AreaTex.h`, `SearchTex.h` : SMAA 1.x de Jorge Jimenez, Belen Masia, Jose I.
  Echevarria, Fernando Navarro et Diego Gutierrez (2011), licence BSD à deux clauses avec mention obligatoire,
  texte complet dans `LICENSE.txt`. Projet d'origine : https://github.com/iryoku/smaa
- Copiés **tels quels** depuis la copie qu'en redistribue DPfix (Durante) dans
  `E:\DeadlyPremonitionFix\third_party\dpfix` (`shaders/SMAA.h`, `src/AreaTex.h`, `src/SearchTex.h`), sans
  modification.
- Mention exigée pour la distribution binaire (licence, clause 2) : « Uses SMAA. Copyright (C) 2011 by Jorge
  Jimenez, Jose I. Echevarria, Belen Masia, Fernando Navarro and Diego Gutierrez. » (reprise dans le README et
  `LICENSE-THIRD-PARTY.txt` du paquet).
- Le point d'entrée Direct3D 9 de HomecomingAgain (`shaders/smaa_d3d9.hlsl`) reprend les fonctions d'enveloppe « DX9_* » du
  `SMAA.fx` de démonstration (même licence), sans le système d'effets D3DX : les shaders sont compilés par `fxc`
  pendant la compilation du mod, ce qui évite toute dépendance à `d3dx9_43.dll`.
