# hyprvibr

Hyprland plugin for achieving the same "vibrant" color effect to X11 libvibrant
and Windows VibranceGUI utility. This tool will dynamically change the Color
Transformation Matrix (CTM) and optionally the resolution of a monitor where a
window that is tracked and focused by the plugin is displayed, and will restore
the original settings when the window is no longer focused.

## Configuration

Using Hyprland Lua configuration:

```lua
hl.plugin.hyprvibr.hyprvibr_app({
    -- Required fields
    class = <app initial class>,
    sat = <saturation value>,

    -- Optional fields
    monitor_mode = {
        w = <width>,
        h = <height>,
        refresh_rate = <refresh rate>
    }
});
```

### Examples

Using Hyprland Lua configuration:

```lua
hl.plugin.hyprvibr.hyprvibr_app({
    class = cs2,
    sat = 3.3,
});
```

or:

```lua
hl.plugin.hyprvibr.hyprvibr_app({
    class = cs2,
    sat = 3.3,
    monitor_mode = {
        w = 1920,
        h = 1080,
    }
});
```

or:

```lua
hl.plugin.hyprvibr.hyprvibr_app({
    class = cs2,
    sat = 3.3,
    monitor_mode = {
        w = 1920,
        h = 1080,
        refresh_rate = 144
    }
});
```

Use `hyprctl clients` to see the current opened windows in Hyprland and check the initial class of each window.

## Compatibility

This plugin is likely to have interactions with other Hyprland clients that
modifies the CTM, for example, via the hyprland_ctm_control_manager_v1 protocol.
Things like hyprsunset will likely have issues running with this plugin.
