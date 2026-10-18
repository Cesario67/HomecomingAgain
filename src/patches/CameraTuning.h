#pragma once

// Calculs du correctif de caméra (patches/CameraFix.h), séparés des hooks pour être testés (tests/unit).
//
// Repris du Camera Fix 1.0 d'Emerson Teles (Emertels), licence MIT, https://github.com/Emertels/Silent-Hill-5-Camera-Fix
// (fichiers navigation_rate_reference.h, camera_speed_wrapper.c et build_navigation_*.py). Les constantes sont les
// siennes, calibrées par ses tests en jeu à la manette ; elles n'ont pas été remesurées ici.

namespace hca::patches::camera {

inline constexpr float kMinSpeed = 0.25f;
inline constexpr float kMaxSpeed = 3.0f;

// Vitesse de rotation horizontale à 1.0, stick à fond, en radians par seconde (environ 86 degrés par seconde).
inline constexpr float kHorizontalReference = 1.5f;
// Le moteur tourne la caméra verticalement à 60 degrés par seconde (double en 0x10F91290, vérifié) puis convertit en
// radians (0.01745329 en 0x10F93F48, vérifié). 1.4323945 = 1.5 / (60 x 0.01745329) aligne la vitesse verticale sur
// l'horizontale ; le facteur 2 est le choix empirique de l'auteur du Camera Fix (son Y 1.0 vaut son ancien Y 2.0).
inline constexpr double kVerticalReferenceGain = 1.4323944989229112;
inline constexpr double kVerticalUserCalibration = 2.0;
// En visée (caméra « Precision »), l'entrée du personnage est doublée puis la sortie de la courbe du moteur est
// multipliée par réglage / 3 (3.0 correspond à la référence rapide des premières versions du Camera Fix).
inline constexpr float kAimInputGain = 2.0f;
inline constexpr float kAimOutputDivisor = 3.0f;
// En dessous de 5 % de déflexion du stick (0,05² = 0,0025), la caméra garde son comportement d'origine.
inline constexpr float kStickDeadzone = 0.05f;

struct CameraSpeeds {
    float horizontal = 1.0f;
    float vertical = 1.0f;
    float aimHorizontal = 1.0f;
    float aimVertical = 1.0f;
};

// Facteurs écrits là où le moteur les lit (voir patches/CameraFix.cpp).
struct CameraGains {
    float yawRadiansPerSecond;  // rotation horizontale du personnage, stick à fond
    double padPitchGain;        // tangage à la manette : remplace le 60 degrés/s du moteur, multiplié par |Y|
    float pitchGain;            // tangage de l'autre branche du moteur : multiplie sa constante (0x111D6368)
    float aimInputGain;         // entrée du personnage en visée
    float aimOutputX;           // sortie horizontale de la courbe de visée
    float aimOutputY;           // sortie verticale de la courbe de visée
};

// Réglage du .ini borné à [0,25 ; 3] ; une valeur non finie donne 1.
float ClampSpeed(float value) noexcept;

CameraGains ComputeGains(const CameraSpeeds& speeds) noexcept;

// Le stick (valeur du moteur, -1 à 1) sort-il de la zone morte du correctif ?
bool IsStickDeflected(float axis) noexcept;

// Même test pour un axe XInput brut (-32768 à 32767).
bool IsRawStickDeflected(short axis) noexcept;

// Rotation demandée au moteur pour une image, en degrés : -(X x durée x vitesse) converti en degrés, signe repris tel
// quel du Camera Fix (validé en jeu par son auteur).
float TurnDegrees(float stickX, float elapsedSeconds, float yawRadiansPerSecond) noexcept;

// Cap (en radians, dans [0 ; 2π[) d'un vecteur avant (x, z) du personnage, comme le moteur le range dans la caméra.
float HeadingFromForward(float forwardX, float forwardZ) noexcept;

// Nom de la caméra de visée du moteur : exactement « Precision ».
bool IsPrecisionCameraName(const char* name) noexcept;

// Mode du stick droit horizontal ([Camera] Mode) : Follow = le personnage tourne et la caméra le suit (Camera Fix),
// Orbit = la caméra tourne seule autour du personnage, sans le ressort qui la ramène derrière lui (expérimental).
enum class CameraMode { Follow, Orbit };

// « Orbit » (sans tenir compte de la casse) donne Orbit ; toute autre valeur, ou rien, donne Follow.
CameraMode ParseCameraMode(const wchar_t* text) noexcept;

// Angle ramené dans [0 ; 2π[.
float WrapHeading(float radians) noexcept;

// Cap de la caméra en mode orbite après une image : inchangé si le stick est dans la zone morte, sinon tourné dans le
// même sens qu'en mode Follow (cap - X x durée x vitesse).
float OrbitYaw(float yaw, float stickX, float elapsedSeconds, float yawRadiansPerSecond) noexcept;

// Au-delà de cette durée sans mise à jour de la caméra de navigation (cinématique, porte, chargement, autre caméra),
// l'orbite repart du cap du moteur, derrière le personnage.
inline constexpr unsigned kOrbitResetMs = 500;

bool OrbitNeedsReset(unsigned long long lastUpdateMs, unsigned long long nowMs) noexcept;

// Déplacement par rapport à la caméra (mode orbite). Le moteur déplace le personnage par rapport à lui-même (stick
// gauche : +0xAD8 avant, +0xADC côté, fonction 0x10384E70, vérifié) et ne le tourne qu'au stick droit : en orbite, le
// mod le tourne vers la direction demandée à l'écran et ne donne au moteur que la marche avant.
//
// [Camera] OrbitMove : Camera = en marche, le personnage se tourne vers le cap de la caméra et le stick gauche garde
// son effet d'origine (avant, arrière, pas de côté) ; Stick = il se tourne vers la direction du stick à l'écran et ne
// reçoit que la marche avant (à la manière de Silent Hill 2 Remake).
enum class OrbitMove { Camera, Stick };

// « Stick » (sans tenir compte de la casse) donne Stick ; toute autre valeur, ou rien, donne Camera.
OrbitMove ParseOrbitMove(const wchar_t* text) noexcept;

// Vitesse maximale de rotation du personnage vers la direction voulue (environ 340 degrés par seconde).
inline constexpr float kTurnRadiansPerSecond = 6.0f;

// Angle ramené dans [-π ; π[.
float SignedAngle(float radians) noexcept;

// Inclinaison du stick gauche, bornée à 1.
float StickMagnitude(float forward, float side) noexcept;

// Cap voulu dans le monde : celui de la caméra pour le stick vers le haut, tourné du même sens que l'orbite pour le
// stick sur le côté (côté positif = droite : hypothèse, à confirmer en jeu).
float CameraRelativeHeading(float forward, float side, float cameraYaw) noexcept;

// Rotation de cette image, bornée à `maxStep` dans les deux sens.
float TurnStep(float error, float maxStep) noexcept;

// Marche avant donnée au moteur : pleine si le personnage regarde la direction voulue, réduite sinon, nulle au-delà
// d'un quart de tour (il pivote d'abord).
float ForwardAfterTurn(float magnitude, float remainingError) noexcept;

}  // namespace hca::patches::camera
