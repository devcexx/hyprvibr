# hyprvibr

Hyprland plugin for achieving the same "vibrant" color effect to X11 libvibrant
and Windows VibranceGUI utility. This tool will dynamically change the Color
Transformation Matrix (CTM) and optionally the resolution of a monitor where a
window that is tracked and focused by the plugin is displayed, and will restore
the original settings when the window is no longer focused.

## Configuration

Using legacy Hyprland configuration mode:

```
plugin {
    hyprvibr {
      hyprvibr-app = <app initial class>, <saturation value>
      hyprvibr-app = <app initial class>, <saturation value>, <width>, <height>
      hyprvibr-app = <app initial class>, <saturation value>, <width>, <height>, <refresh rate>
      hyprvibr-app = ...
    }
}
```

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

Using legacy Hyprland configuration mode:

```
plugin {
    hyprvibr {
      # Just saturation
      hyprvibr-app = cs2, 3.3

      # Saturation + resolution
      hyprvibr-app = cs2, 1.5, 1920, 1080

      # Saturation + resolution + refresh rate
      hyprvibr-app = cs2, 1.2, 2560, 1440, 144
    }
}
```

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
