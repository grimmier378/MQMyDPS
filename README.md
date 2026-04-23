# MQMyDPS

A native C++ MacroQuest plugin for EverQuest that provides real-time DPS tracking, battle history, healing statistics, floating combat text (FCT), and graphing via ImPlot.

## Features

- Real-time DPS tracking with per-target breakdown
- Battle history with expandable per-target details
- Healing tracking (direct + crit heals) with pie charts
- Floating Combat Text (FCT) anchored to 3D spawn positions
- Customizable per-type colors and icons for FCT
- ImPlot line/bar/pie graphs
- Combat Output overlay (scrolling damage feed)
- MQMyChat integration for battle reports
- TLO API for macro/Lua access (`${MyDPS}`)
- Per-character INI settings persistence

## Screenshots

### Battle History

| Cleric | Shadowknight |
|--------|-------------|
| ![Cleric battle history](https://raw.githubusercontent.com/grimmier378/MQMyDPS/main/Documentation/media/cleric.png) | ![Shadowknight battle history](https://raw.githubusercontent.com/grimmier378/MQMyDPS/main/Documentation/media/shadowknight.png) |

### Graphs

![Graphs tab](https://raw.githubusercontent.com/grimmier378/MQMyDPS/main/Documentation/media/graphs.png)

### FCT Configuration

![FCT settings](https://raw.githubusercontent.com/grimmier378/MQMyDPS/main/Documentation/media/settings.png)

### Floating Combat Text

<video src="https://raw.githubusercontent.com/grimmier378/MQMyDPS/main/Documentation/media/fct_demo.mp4" controls width="600"></video>

## Getting Started

```
/plugin MQMyDPS
```

## Commands

| Command | Description |
|---------|-------------|
| `/mydps` | Toggle main window |
| `/mydps start` | Begin tracking |
| `/mydps stop` | Stop tracking |
| `/mydps reset` | Clear all data |
| `/mydps report` | Print session summary |
| `/mydps show` / `hide` | Show or hide main window |
| `/mydps spam` | Toggle combat output overlay |
| `/mydps lock` | Toggle click-through on overlay |
| `/mydps fct` | Toggle floating combat text |
| `/mydps config` | Toggle config window |

## Configuration

Settings are stored per-character at:

```
{MQConfigDir}\MQMyDPS\{Server}\{CharName}.ini
```

INI sections: `Options`, `Windows`, `FCT`, `FCTIcons`, `Colors`.

## TLO API

Access plugin data from macros or Lua via the `${MyDPS}` TLO:

```
${MyDPS.DPS}
${MyDPS.InCombat}
${MyDPS.Target[name].TotalDamage}
```

## Documentation

Full technical documentation is available at [Documentation/index.html](Documentation/index.html).

## Dependencies

MQ2Main, ImGui, ImPlot, fmt, eqlib -- all included in the MacroQuest build environment.

## Author

Grimmier
