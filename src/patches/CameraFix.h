#pragma once

#include <windows.h>

namespace hca::patches {

// Correctif de caméra à la manette, porté du Camera Fix 1.0 d'Emerson Teles (Emertels, licence MIT,
// https://github.com/Emertels/Silent-Hill-5-Camera-Fix). Le mod d'origine distribue un g_SilentHill.sgl modifié ;
// ici, les mêmes modifications sont appliquées en mémoire au démarrage, et le fichier du jeu n'est pas touché.
//
// Cinq effets, tous réglés par la section [Camera] du .ini (voir patches/CameraTuning.h) :
// - stick droit horizontal : le personnage tourne avec la caméra, à une vitesse réglable, et la caméra suit son cap ;
// - stick droit vertical : réponse proportionnelle au lieu de la courbe au carré du moteur ;
// - visée (caméra « Precision ») : sensibilités horizontale et verticale indépendantes.
//
// Sites, g_SilentHill.sgl Steam build 6.30 (horodatage PE 0x490F50BF). Les instructions d'origine ont été vérifiées
// par désassemblage le 14/10/2026 (identiques à celles que le Camera Fix remplace dans son propre moteur) ;
// le rôle des registres et des champs vient de l'analyse du Camera Fix, confirmé seulement là où c'est indiqué :
// - 0x108DB3D6 (movss [esp+28h],xmm0) : rangement du cap de la caméra de navigation (TCamComNavigationSHV) ;
//   esi = composant caméra (+0x11C cap, +0x120 vitesse de suivi), edi = transformation du personnage suivi (vérifié :
//   chargée en 0x108DAC1E et inchangée jusqu'au site), [ebp+0Ch] = commandes de caméra (+0x20 X, +0x24 Y),
//   [esp+24h] = durée de l'image ;
// - 0x108DAFFB (mulsd xmm1,[0x10F91290]) : tangage à la manette, Y² x 60 degrés par seconde (vérifié) ;
// - 0x108DB012 (movss xmm1,[0x111D6368]) : tangage de l'autre branche, Y x constante (vérifié) ;
// - 0x103851E0 (movss xmm1,[esi+0AD8h]) : entrée du personnage ; +0xAE8 et +0xAEC servent à la visée ;
// - 0x10711F24 et 0x1071EA28 : sorties horizontale ([esp+20h]) et verticale (xmm0) de la courbe de visée ;
// - 0x10145F50 : rotation d'un personnage, cdecl (personnage, degrés) (prologue vérifié) ;
// - 0x1158982C : gestionnaire de caméras, dont on lit le nom de la caméra active.
//
// Mode orbite, expérimental ([Camera] Mode=Orbit, 17/10/2026) : au même site 0x108DB3D6, le stick fait tourner le cap
// de la caméra seul (le personnage n'est plus tourné) et le cap calculé par le moteur, qui ramène la caméra derrière le
// personnage (ressort de 0x108DB27A à 0x108DB39E, vérifié), est ignoré. Le moteur tourne aussi le personnage avec le
// stick droit par son entrée (+0xAE8, recopiée en 0x10384F57) : mise à 0 hors visée. Le stick gauche le déplace par
// rapport à lui-même (vérifié en jeu le 18/10/2026) : en marche, le mod le tourne vers la caméra (stick d'origine
// conservé). Validé en jeu à la manette le 18/10/2026. Pendant la garde
// au couteau (L2), l'orbite reste active : une option pour la suspendre est notée dans docs/a-faire.md.
//
// Désactivé par défaut ([Camera] CameraFix=0) : le comportement change et il faut retirer du stick droit les
// commandes COMMAND_TURNLEFT et COMMAND_TURNRIGHT (Engine\binds_pc_mjs.cfg), sinon le personnage tourne deux fois.
// Les effets ne s'appliquent que quand le stick droit de la première manette est incliné, pour laisser la souris
// telle quelle (hypothèse : rien ne prouve que la souris passe par d'autres sites).
void ApplyCameraFix(HMODULE engineModule);

// Appelée par le hook XInput (input/XInputHooks.cpp) à chaque lecture de la première manette.
void NoteRightStick(SHORT x, SHORT y) noexcept;

// Gâchette gauche de la première manette (0 à 255) : tenue, elle suspend l'orbite ([Camera] OrbitGuardFollow=1).
void NoteLeftTrigger(BYTE value) noexcept;

}  // namespace hca::patches
