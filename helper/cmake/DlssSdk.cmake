# SDK DLSS de NVIDIA v310.9.1 (licence NVIDIA RTX SDKs, voir README : permission supplémentaire GPL article 7).
# Jamais stocké dans le dépôt : chaque fichier utile est téléchargé depuis le dépôt officiel NVIDIA/DLSS, vérifié par
# son empreinte SHA-256, dans le dossier de compilation. Définit DLSS_SDK_DIR.

set(DLSS_SDK_VERSION v310.9.1)
set(DLSS_SDK_DIR "${CMAKE_BINARY_DIR}/dlss-sdk-${DLSS_SDK_VERSION}")
set(DLSS_SDK_URL "https://raw.githubusercontent.com/NVIDIA/DLSS/${DLSS_SDK_VERSION}")

set(DLSS_SDK_FILES
    "LICENSE.txt" D4216E39EBEF5F9B50A6712EBB37BEEB5379862A67733A9999C651F21592AAF0
    "include/nvsdk_ngx.h" DC38E7467CF415379C9D12AE1B6E4A494C453ED92720FB53E92AECB523E7B848
    "include/nvsdk_ngx_defs.h" EA23F33497CD274860D1C25A97644FCE807DCB0037C594547203343103FAD03E
    "include/nvsdk_ngx_defs_dlssd.h" 2E97FE1595B1334F71B51724F2CB67496E8C296A7A2EEC5606DA53055882E716
    "include/nvsdk_ngx_defs_dlssg.h" 98EC3A6FAA5AA250828C174BF8F3491AAFAC693DFE57CC798049C8918597FC60
    "include/nvsdk_ngx_defs_vk.h" 0A24D0861ACE7D6B9362A67B7F08BEA1B33EA123C5CE730B19133DE8A891D031
    "include/nvsdk_ngx_helpers.h" 5BCBADFE7478B802CF6D3ACA4DC5DDD7D0889B99726E69C63F9E9BD555F44471
    "include/nvsdk_ngx_helpers_vk.h" C192BFF72138F12F770DB48E12C1D8F712DFC81EA0E8E055EBB27D8DD1B31623
    "include/nvsdk_ngx_params.h" 943BC8CC5CDAE03B6303016FBAD3183636F2335AE27A2D18776798C3B4EFABBC
    "include/nvsdk_ngx_params_dlssd.h" D41E3BA8F1D72583B55273092C99BF426DC4A0D46DE827229E25D5B1325A370E
    "include/nvsdk_ngx_params_dlssg.h" C24FA5AA68FFA2808C828B75CFA32843F1606F80A145F84055AC450D7E6C74AD
    "include/nvsdk_ngx_vk.h" 2D364CE7132881EB669E9498FD570D74CBA563B1FBEBC82C235F8E1AD2DD8B6D
    "lib/Windows_x86_64/x64/nvsdk_ngx_s.lib" 4E5D355086D2BC11E1A0842457D2519EA528EE1F3E112C45679A84960C07DFF3
    "lib/Windows_x86_64/rel/nvngx_dlss.dll" 3975567B8943C53ACCE397F2B72380092F84F162D00B0D2C7D08A1025C563983)

list(LENGTH DLSS_SDK_FILES _count)
math(EXPR _last "${_count} - 1")
foreach(_index RANGE 0 ${_last} 2)
    math(EXPR _hashIndex "${_index} + 1")
    list(GET DLSS_SDK_FILES ${_index} _file)
    list(GET DLSS_SDK_FILES ${_hashIndex} _hash)
    if(EXISTS "${DLSS_SDK_DIR}/${_file}")
        file(SHA256 "${DLSS_SDK_DIR}/${_file}" _existing)
        string(TOUPPER "${_existing}" _existing)
        if(NOT _existing STREQUAL _hash)
            file(REMOVE "${DLSS_SDK_DIR}/${_file}")
        endif()
    endif()
    if(NOT EXISTS "${DLSS_SDK_DIR}/${_file}")
        message(STATUS "SDK DLSS : téléchargement de ${_file}")
        file(DOWNLOAD "${DLSS_SDK_URL}/${_file}" "${DLSS_SDK_DIR}/${_file}" EXPECTED_HASH SHA256=${_hash} TLS_VERIFY ON
             STATUS _status)
        list(GET _status 0 _code)
        if(NOT _code EQUAL 0)
            message(FATAL_ERROR "SDK DLSS : téléchargement de ${_file} impossible (${_status})")
        endif()
    endif()
endforeach()
