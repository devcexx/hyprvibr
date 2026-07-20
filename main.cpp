#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/SharedDefs.hpp>
#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/protocols/core/Compositor.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <array>
#include <format>
#include <hyprutils/string/ConstVarList.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/config/shared/monitor/MonitorRule.hpp>
#include <hyprland/src/config/shared/monitor/MonitorRuleManager.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/lua/bindings/LuaBindingsInternal.hpp>
#include <luaconf.h>
#include "globals.hpp"

extern "C" {
#include <lua.h>
}

using namespace Hyprutils::String;

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

struct SAppConfig {
    std::string szClass;
    float sat;
    int resX = -1;
    int resY = -1;
    float refreshRate = -1.0f;
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
int g_activeResX = -1;
int g_activeResY = -1;
std::optional<Config::CMonitorRule> g_originalMonitorRule;

// Evily stoled from libvibrant
const Mat3x3 calc_ctm_matrix(float sat) {
    std::array<float, 9> mat;
    float coeff = (1.0 - sat) / 3.0;
    for (int i = 0; i < 9; i++) {
        mat[i] = coeff + (i % 4 == 0 ? sat : 0);
    }
    return mat;
}

static void pushAppConfig(SAppConfig cfg) {
    Log::logger->log(Log::INFO, "[hyprvibr] Configuration added for class {}: sat {}, res {}x{}@{}", cfg.szClass, cfg.sat, cfg.resX, cfg.resY, cfg.refreshRate);
    g_appConfigs.emplace_back(std::move(cfg));
}

static int fillLuaMonitorMode(lua_State* L, SAppConfig& config) {
    if (!lua_istable(L, -1)) {
        Log::logger->log(Log::ERR, "[hyprvibr] Is not a table");
        return Config::Lua::Bindings::Internal::configError(L, "hyprvibr_app: 'monitor' field, if specified, must be a table");
    }

    Log::logger->log(Log::ERR, "[hyprvibr] Type: {}", lua_type(L, 1));

    {
        Hyprutils::Utils::CScopeGuard x([L] { lua_pop(L, 1); });

        lua_getfield(L, -1, "w");
        if (!lua_isnil(L, -1)) {
            if (!lua_isinteger(L, -1)) {
                return Config::Lua::Bindings::Internal::configError(L, "hyprvibr_app: 'monitor' field: 'w' field must be an integer");
            }
            config.resX = lua_tointeger(L, -1);
            if (config.resX < 0) {
                return Config::Lua::Bindings::Internal::configError(L, "hyprvibr_app: 'monitor' field: 'w' field must be non-negative");
            }
        }
    }

    {
        Hyprutils::Utils::CScopeGuard x([L] { lua_pop(L, 1); });

        lua_getfield(L, -1, "h");
        if (!lua_isnil(L, -1)) {
            if (!lua_isinteger(L, -1)) {
                return Config::Lua::Bindings::Internal::configError(L, "hyprvibr_app: 'monitor' field: 'h' field must be an integer");
            }

            config.resY = lua_tointeger(L, -1);
            if (config.resY < 0) {
                return Config::Lua::Bindings::Internal::configError(L, "hyprvibr_app: 'monitor' field: 'h' field must be non-negative");
            }
        }
    }

    if (config.resY < 0 && config.resX >= 0 || config.resX < 0 && config.resY >= 0) {
        return Config::Lua::Bindings::Internal::configError(L, "hyprvibr_app: 'monitor' field: both 'w' and 'h' fields must be specified together");
    }

    {
        Hyprutils::Utils::CScopeGuard x([L] { lua_pop(L, 1); });
        lua_getfield(L, -1, "refresh_rate");
        if (!lua_isnil(L, -1)) {
            if (!lua_isnumber(L, -1)) {
                return Config::Lua::Bindings::Internal::configError(L, "hyprvibr_app: 'monitor' field: 'refresh_rate' field must be a number");
            }
            config.refreshRate = (float)lua_tonumber(L, -1);
        }
    }

    return 0;
}

static int luaAddApp(lua_State* L) {
    if (!lua_istable(L, 1)) {
        return Config::Lua::Bindings::Internal::configError(L, "hyprvibr_app: a table is required");
    }

    SAppConfig config;
    int r;

    {
        Hyprutils::Utils::CScopeGuard x([L] { lua_pop(L, 1); });

        lua_getfield(L, 1, "class");
        if (!lua_isstring(L, -1)) {
            return Config::Lua::Bindings::Internal::configError(L, "hyprvibr_app: 'class' field must be a string");
        }

        config.szClass = lua_tostring(L, -1);
    }

    {
        Hyprutils::Utils::CScopeGuard x([L] { lua_pop(L, 1); });

        lua_getfield(L, 1, "sat");
        if (!lua_isnumber(L, -1)) {
            return Config::Lua::Bindings::Internal::configError(L, "hyprvibr_app: 'sat' field must be a number");
        }

        config.sat = (float)lua_tonumber(L, -1);
    }

    {
        Hyprutils::Utils::CScopeGuard x([L] { lua_pop(L, 1); });
        lua_getfield(L, 1, "monitor_mode");
        if (!lua_isnil(L, -1)) {
            Log::logger->log(Log::ERR, "[hyprvibr] Monitor mode is not nil");
            if ((r = fillLuaMonitorMode(L, config)) != 0)
                return r;
        }
    }

    pushAppConfig(config);
    return 0;
}

void onActiveWindowChange(const PHLWINDOW win) {
    if (win) {
        Log::logger->log(Log::TRACE, "[hyprvibr] Active window change: {} ({})", win->m_title, win->m_initialClass);
    } else {
        Log::logger->log(Log::TRACE, "[hyprvibr] No active window");
    }
    const auto CONFIG = win ? getAppConfig(win->m_initialClass) : nullptr;
    auto prevMon = g_activeMonitor.lock();
    PHLMONITOR newMon;
    float newSat;
    int newResX = -1;
    int newResY = -1;

    if (CONFIG == nullptr) {
        g_activeMonitor = {};
        newMon = {};
        newSat = 0;
    } else {
        g_activeMonitor = win->m_monitor;
        newMon = win->m_monitor.lock();
        newSat = CONFIG->sat;
        newResX = CONFIG->resX;
        newResY = CONFIG->resY;
    }

    bool settingsChanged = prevMon != newMon || newSat != g_activeMonitorSat || newResX != g_activeResX || newResY != g_activeResY;

    if (settingsChanged) {
        if (prevMon && prevMon != newMon) {
            prevMon->setCTM(Mat3x3::identity());

            if (g_originalMonitorRule.has_value()) {
                Config::monitorRuleMgr()->add(std::move(g_originalMonitorRule.value()));
                Log::logger->log(Log::INFO, "[hyprvibr] Restored monitor {}", prevMon->m_name);
                g_originalMonitorRule.reset();
            }
        }

        if (newMon) {
            if (newSat != g_activeMonitorSat) {
                newMon->setCTM(calc_ctm_matrix(CONFIG->sat));
            }

            if (CONFIG->resX > 0 && CONFIG->resY > 0) {
                auto currentResX = (int)newMon->m_pixelSize.x;
                auto currentResY = (int)newMon->m_pixelSize.y;

                if (!g_originalMonitorRule.has_value()) {
                    g_originalMonitorRule = newMon->m_activeMonitorRule;
                }

                if (currentResX != CONFIG->resX || currentResY != CONFIG->resY) {
                    float refreshRate = CONFIG->refreshRate > 0 ? CONFIG->refreshRate : 60.0f;
                    Config::CMonitorRule newRule = newMon->m_activeMonitorRule;
                    newRule.m_resolution = Vector2D((float)CONFIG->resX, (float)CONFIG->resY);
                    newRule.m_refreshRate = refreshRate;
                    Config::monitorRuleMgr()->add(std::move(newRule));
                    Log::logger->log(Log::INFO, "[hyprvibr] Scheduled monitor resolution change to {}x{}@{} on {}", CONFIG->resX, CONFIG->resY, refreshRate, newMon->m_name);
                }
            } else if (g_activeResX > 0 && g_activeResY > 0 && g_originalMonitorRule.has_value()) {
                Config::monitorRuleMgr()->add(std::move(g_originalMonitorRule.value()));
                Log::logger->log(Log::INFO, "[hyprvibr] Restored monitor {}", prevMon->m_name);
                g_originalMonitorRule.reset();
            }
        }

        g_activeMonitorSat = newSat;
        g_activeResX = newResX;
        g_activeResY = newResY;
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

    static auto P = Event::bus()->m_events.window.active.listen([](PHLWINDOW WIN, Desktop::eFocusReason reason) {
        onActiveWindowChange(WIN);
    });

    static auto P2 = Event::bus()->m_events.config.preReload.listen([]() {
        g_appConfigs.clear();
    });

    static auto P3 = Event::bus()->m_events.config.reloaded.listen([]() {
        onActiveWindowChange(Desktop::focusState()->window());
    });

    static auto P4 = Event::bus()->m_events.window.destroy.listen([](PHLWINDOWREF WIN) {
        onActiveWindowChange(Desktop::focusState()->window());
    });

    if (Config::mgr()->type() == Config::CONFIG_LEGACY) {
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
            HyprlandAPI::addConfigKeyword(
                PHANDLE, "hyprvibr-app",
                [](const char* l, const char* r) -> Hyprlang::CParseResult {
                    const std::string      str = r;
                    CConstVarList          data(str, 0, ',', true);

                    Hyprlang::CParseResult result;

                    if (data.size() < 2 || data.size() > 5) {
                        result.setError("hyprvibr-app requires 2-5 params: class,sat[,resX,resY[,refreshRate]]");
                        return result;
                    }

                    try {
                        SAppConfig config;
                        config.szClass = data[0];
                        config.sat = std::stof(std::string{data[1]});

                        if (data.size() >= 4) {
                            config.resX = std::stoi(std::string{data[2]});
                            config.resY = std::stoi(std::string{data[3]});
                        }

                        if (data.size() >= 5) {
                            config.refreshRate = std::stof(std::string{data[4]});
                        }

                        pushAppConfig(config);
                    } catch (std::exception& e) {
                        result.setError("failed to parse line");
                        return result;
                    }

                    return result;
                },
                Hyprlang::SHandlerOptions{});
        #pragma GCC diagnostic pop
    } else if (Config::mgr()->type() == Config::CONFIG_LUA) {
        HyprlandAPI::addLuaFunction(PHANDLE, "hyprvibr", "hyprvibr_app", ::luaAddApp);
    } else {
        HyprlandAPI::addNotification(PHANDLE, "[hyprvibr] Failure in initialization: Unrecognized Hyprland configuration type",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[hyprvibr] Unsupported Hyprland configuration type");
    }

    HyprlandAPI::addNotification(PHANDLE, "Hyprvibr loaded", CHyprColor{0.2, 1.0, 0.2, 1.0}, 5000);
    return {"hyprvibr", "A plugin to customize monitor saturation per focused window", "devcexx", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    onActiveWindowChange({});
}
