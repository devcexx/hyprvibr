#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/SharedDefs.hpp>
#include <hyprland/src/debug/Log.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/Monitor.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/protocols/core/Compositor.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <any>
#include <array>
#include <hyprutils/string/ConstVarList.hpp>
#include <hyprland/src/desktop/view/Window.hpp>

#include "globals.hpp"

using namespace Hyprutils::String;

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

struct SAppConfig {
    std::string szClass;
    float sat;
};

std::vector<SAppConfig>  g_appConfigs;

static const SAppConfig* getAppConfig(const std::string& appClass) {
    for (const auto& ac : g_appConfigs) {
        if (ac.szClass != appClass)
            continue;
        return &ac;
    }
    return nullptr;
}

PHLMONITORREF g_activeMonitor;
float g_activeMonitorSat;

// Evily stoled from libvibrant
const Mat3x3 calc_ctm_matrix(float sat) {
    std::array<float, 9> mat;
    float coeff = (1.0 - sat) / 3.0;
    for (int i = 0; i < 9; i++) {
        mat[i] = coeff + (i % 4 == 0 ? sat : 0);
    }
    return mat;
}

void onActiveWindowChange(const PHLWINDOW win) {
    if (win) {
        Debug::log(TRACE, "[hyprvibr] Active window change: {} ({})", win->m_title, win->m_initialClass);
    } else {
        Debug::log(TRACE, "[hyprvibr] No active window");
    }
    const auto CONFIG = win ? getAppConfig(win->m_initialClass) : nullptr;
    auto prevMon = g_activeMonitor.lock();
    PHLMONITOR newMon;
    float newSat;

    if (CONFIG == nullptr) {
        g_activeMonitor = {};
        newMon = {};
        newSat = 0;
    } else {
        g_activeMonitor = win->m_monitor;
        newMon = win->m_monitor.lock();
        newSat = CONFIG->sat;
    }

    if (prevMon != newMon || newSat != g_activeMonitorSat) {
        if (prevMon && prevMon != newMon) {
            prevMon->setCTM(Mat3x3::identity());
            Debug::log(INFO, "[hyprvibr] Removed custom CTM from monitor {}", prevMon->m_name);
        }

        if (newMon) {
            newMon->setCTM(calc_ctm_matrix(CONFIG->sat));
            Debug::log(INFO, "[hyprvibr] Set custom CTM to monitor {}", newMon->m_name);
        }

        g_activeMonitorSat = newSat;
    }
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH        = __hyprland_api_get_hash();
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (HASH != CLIENT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[hyprvibr] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[hyprvibr] Version mismatch");
    }

    static auto P = HyprlandAPI::registerCallbackDynamic(PHANDLE, "activeWindow", [&](void* self, SCallbackInfo& info, std::any data) {
        const auto WIN = std::any_cast<PHLWINDOW>(data);
        onActiveWindowChange(WIN);
    });

    static auto P2 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "preConfigReload", [&](void* self, SCallbackInfo& info, std::any data) {
        g_appConfigs.clear();
    });

    static auto P3 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "configReloaded", [&](void* self, SCallbackInfo& info, std::any data) {
        onActiveWindowChange(Desktop::focusState()->window());
    });

    HyprlandAPI::addConfigKeyword(
        PHANDLE, "hyprvibr-app",
        [](const char* l, const char* r) -> Hyprlang::CParseResult {
            const std::string      str = r;
            CConstVarList          data(str, 0, ',', true);

            Hyprlang::CParseResult result;

            if (data.size() != 2) {
                result.setError("hyprvibr-app requires 2 params");
                return result;
            }

            try {
                SAppConfig config;
                config.szClass = data[0];
                config.sat = std::stof(std::string{data[1]});
                g_appConfigs.emplace_back(std::move(config));
            } catch (std::exception& e) {
                result.setError("failed to parse line");
                return result;
            }

            return result;
        },
        Hyprlang::SHandlerOptions{});

    HyprlandAPI::addNotification(PHANDLE, "Hyprvibr loaded", CHyprColor{0.2, 1.0, 0.2, 1.0}, 5000);
    return {"hyprvibr", "A plugin to customize monitor saturation per focused window", "devcexx", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    onActiveWindowChange({});
}
