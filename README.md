Luvvy Dishonored Independent Overlay v0.8.0

The proven F10 overlay remains independent from Dishonored's Esc / Scaleform menu.

MENU
Immortal
Never Detected
Infinite Mana
Infinite Ammo
Infinite Potions
No Blink Cooldown
Long Blink 2x
Infinite Oxygen
Infinite Health Elixirs
Infinite Mana Elixirs
Infinite Dark Vision
Infinite Possession
Infinite Bend Time
Refill All Ammo
Add 1000 All Resources
Add 10000 Coins
Add 10 Runes
Give Bone Charm
Max Powers
Max Upgrades
Kill All Rats

CONTROLS
F10 = open / close
Up / Down = navigate
Enter = toggle or run selected action
Esc = close overlay

LONG BLINK FIX
v0.7 tried to locate 1400/600 range floats in the live Blink component and failed.
v0.8 no longer does that. It sends Dishonored's own UE3 set commands:
ON  -> 2800 horizontal / 1200 vertical
OFF -> 1400 horizontal / 600 vertical

RESOURCE ACTIONS
Coins, Runes, Bone Charm, Max Powers, Max Upgrades and Kill All Rats use hidden
Ctrl+Shift function-key bindings installed by Luvvy-Launch.bat. Each binding
enables DishonoredCheatManager before executing the command.

Add 1000 All Resources directly walks Dishonored's resource array and adds 1000
to every currently registered resource entry.

STARTUP
Luvvy-Launch.bat still skips the startup logos/legal movies and keeps
bPauseOnLossOfFocus=FALSE.

INSTALL
1. Fully close Dishonored.
2. Extract into the game root.
3. Overwrite Binaries\Win32\Mods\ModLoader.dll.
4. Launch with Luvvy-Launch.bat.
5. Load gameplay and press F10.
