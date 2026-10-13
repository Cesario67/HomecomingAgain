#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "SharedFrameReader.h"

namespace hca::dlss {

// DLSS de NVIDIA (SDK NGX, Vulkan) en mode DLAA : même résolution en entrée et en sortie, le DLSS ne sert qu'à
// l'anticrénelage temporel. Entrées : la scène, la profondeur et les vecteurs de mouvement partagés par le mod (images
// de l'anneau, SharedFrameReader), plus le décalage de caméra de chaque image. Sortie : une image R8G8B8A8 propre à ce
// processus. Équivalent C# : une classe IDisposable qui possède la fonction DLSS et ses images.
//
// SDK sous licence NVIDIA RTX SDKs (voir README, permission supplémentaire GPL article 7).
class DlssUpscaler {
public:
    using Logger = std::function<void(const std::string&)>;

    explicit DlssUpscaler(Logger log);
    ~DlssUpscaler();
    DlssUpscaler(const DlssUpscaler&) = delete;
    DlssUpscaler& operator=(const DlssUpscaler&) = delete;

    // Extensions Vulkan exigées par le DLSS, à activer à la création de l'instance et du périphérique.
    static bool RequiredExtensions(std::vector<std::string>& instanceExtensions,
                                   std::vector<std::string>& deviceExtensions);

    // Initialise NGX et crée la fonction DLSS. `dllDirectory` : dossier de nvngx_dlss.dll ; `dataDirectory` : journaux
    // de NGX. False si le DLSS n'est pas disponible (carte non NVIDIA RTX, pilote trop ancien, DLL absente).
    bool Initialize(const VulkanContext& context, const Arguments& args, const std::vector<SlotImages>& slots,
                    const std::wstring& dllDirectory, const std::wstring& dataDirectory);

    // Exécute le DLSS sur l'emplacement `slot` et attend la fin du travail sur la carte. `jitter` en pixels.
    // `value` : numéro de l'image, passé au compteur « traitées » quand le mod attend le retour de l'image.
    bool Evaluate(std::size_t slot, std::uint64_t value, float jitterX, float jitterY, bool reset);

    // Copie la sortie de l'emplacement en mémoire (BGRA 8 bits, pour WriteColorBmp).
    bool ReadOutput(std::size_t slot, std::vector<std::uint8_t>& bgra);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace hca::dlss
