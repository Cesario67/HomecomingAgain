#pragma once

namespace hca::graphics {

// Paramètres de la projection perspective du jeu : xScale, yScale, et z/w = a + b / zVue.
struct Projection {
    float xScale = 1.6407f;  // valeurs relevées dans le prologue (10/10/2026), utilisées tant qu'aucune n'est lue
    float yScale = 2.9167f;
    float a = 1.0002f;
    float b = -3.0006f;
};

// Reconnaît une matrice de projection perspective telle que le moteur l'envoie (4 registres, une ligne de la
// matrice transposée par registre) : [xs 0 0 0] [0 ys 0 0] [0 0 a b] [0 0 1 0]. Fonction pure, testée par tests/unit.
bool ParseProjection(const float* registers, Projection& projection) noexcept;

}  // namespace hca::graphics
