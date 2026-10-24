# The Adventures of Sparky and Leo

An N64-style 3D platformer for Unreal Engine 5.2, based on the story in
`The_Adventures_of_Sparky_and_Leo.pdf`. Sparky is a genius cat rocket scientist from the Catnip
Sector; Leo is the most extraordinary average person in the galaxy. This first version covers
chapters 1–9. Like the other games here, it's built entirely from code: procedural meshes in C++,
textures and audio synthesized by Python scripts.

```bash
./run_game.sh                  # play (title screen -> story -> chapter 1)
./run_game.sh -Chapter=3       # start at chapter 3 (its story pages first; add -SkipStory to skip them)
./run_editor.sh                # open in the editor
./build.sh                     # recompile after editing Source/ (editor closed)
./build_content.sh             # re-synthesize audio + textures and import them
./package.sh                   # standalone build in Packaged/Linux (run it with: bash Play_SparkyLeo.sh)
```

## Sharing with friends on Windows

This machine's Unreal Engine is the Linux build, and Unreal can't build Windows games from Linux.
The Windows version has to be built once on a Windows PC:

1. `./make_windows_kit.sh` here writes `SparkyLeo-WindowsKit.zip` (code, content and config,
   about 30 MB). Copy it to the Windows PC and unzip it.
2. On that PC, install **Unreal Engine 5.2** from the Epic Games Launcher, and **Visual Studio 2022**
   with the "Game development with C++" workload. Both are free.
3. Double-click `package_windows.bat`. It takes 20–60 minutes the first time and writes
   `SparkyAndLeo-Windows.zip`.
4. Send that zip to your friends. They unzip it and double-click `SparkyLeo.exe`; `HOW_TO_PLAY.txt`
   inside has the controls and what to do if Windows warns about it.

On Windows the game plays with keyboard, keyboard + mouse, or an Xbox-style controller. The USB
N64 pad support reads Linux's `/dev/input`, so it's Linux-only, and the calibration menu item is
hidden on Windows. For friends on Linux, `./package.sh` makes a standalone Linux build here.

## Controls

| | N64 controller | keyboard + mouse | keyboard only | gamepad |
|---|---|---|---|---|
| move | stick (or D-pad) | WASD / arrows | WASD | left stick |
| jump (again in the air: somersault) | A | Space | Space | A |
| swipe / talk / weld (hold); in the air: pounce | B | J, F or left click | J or F | X |
| in the air: ground pound; held + jump: backflip / long jump | Z | Shift, K or right click | Shift or K | LT / RT |
| camera | C-Left / C-Right turn, C-Up / C-Down zoom | mouse, Q / E, wheel | arrows turn and tilt, Q / E, I / O zoom | right stick |
| recentre camera | R or L | Tab or L | Tab or L | LB / RB |
| pause | Start | Enter / Esc / P | Enter / Esc / P | Start |

Pick **CONTROLS: KEYBOARD ONLY** on the title or pause menu (or run with `-KeyboardOnly`) to
leave the mouse alone and steer the camera with the arrow keys. The camera follows Sparky on its
own while he runs, but it never swings round mid-jump, so jumps go where the stick points. If
something gets between it and Sparky, it tilts up over the obstacle.

- Jump at a **velcro wall** (the fuzzy pink ones) and Sparky sticks to it; jump again to leap off,
  wall to wall.
- Land on enemies' heads to stomp them. A ground pound hits harder and knocks pirate pods loose.
- Sparky has 5 paws of health and 9 lives (he's a cat). Every 10 catnip restores a paw, and 50 give
  a life.

The N64 pad is read straight from `/dev/input/js*`, using the same code as Cheeto 64. If a pad's
buttons come out scrambled, use **CALIBRATE CONTROLLER** on the title or pause menu. The result is
saved per controller in `Saved/SparkyLeo_Pads.ini`. Settings and chapter progress are saved in
`Saved/SparkyLeo.ini`.

## The chapters

1. **Space Cats Unite** – Cranberry Plasma. The atom crunch drive 6000 is on the fritz. Only the
   high-hatted may trade here, so find five blocks of styrofoam (up the old hat tower, up the
   velcro party hall, across the bog on sinking lily pads, up the floating plasma rocks, atop jelly
   hill), and Sparky crafts a hat that is aesthetically pleasing and mechanically brilliant. Then
   collect the drive part, peanut butter, cat food, Tang and soup, and get back to the ship. Leo
   tags along (and takes "shortcuts" when he falls behind).
2. **Revelations and Reflections** – a flashback to Chinatown, San Francisco, where Sparky was a
   cage fighter. Up the alley, into the cage, three rounds: two alley cats, four alley cats, then
   Meatball, who leaps and slams out shockwaves (jump them, then hit him while he's dizzy for double
   damage). Afterwards a kid waits by the gate.
3. **Pirates!** – Captain Crotch Sniffer attacks. Leo mans the guns (no opposable thumbs needed)
   while Sparky, in low gravity on the hull, knocks three boarding pods loose with ground pounds,
   past pirate dogs and laser strikes, then runs for the cockpit and the hyperspace jump.
4. **Crisis and Repair** – the ship was damaged in the battle. Oxygen is draining, gravity is
   failing (jumps get floatier, crates drift up) and the floors are icing over. Get the welder off
   the shelf above the bed, weld the four broken conduits (hold B), then fix the climate module in
   the engine room before the oxygen runs out. Leo reports the oxygen levels from the cockpit.

5. **Fluffer-Nutter** – a lush green planet with a village of simple, friendly people. In the
   forest Leo finds Fluffernutter, and Sparky's instincts say "prey". Fluffernutter flees, so find
   three moonberries (up the great tree, beside the falls, out on the lagoon rock over floating
   logs), then **sneak** up on him (hold crouch to creep) until he trusts you. Run at him and he
   bolts. He then joins as a second follower.
6. **Fluffernutter's Revenge** – the pirates board the ship and Fluffernutter transforms. You
   play as him, a hulking brawler whose swipes and ground pounds hit harder and wider: smash
   two waves of pirate dogs, then Captain Crotch Sniffer himself, who swings, charges, and
   calls for backup at half health.
7. **The Labyrinth of Time** – floating walkways in the void. West: narrow paths with temporal
   rifts drifting across. East: float across a gap inside a gravity distortion. Through the
   wormhole: moving stones and a climb. Three time shards open the heart. Echoes of Sparky, Leo
   and Fluffernutter from other timelines give hints.
8. **Riddles of the Ancients** – a hub with three portals: the lost scientist's quantum tiles
   (each step flips a tile and its neighbours, so light them all), the warrior princess's arena
   (two waves of ghostly wraiths), and three riddle stones (multiple choice, and a wrong answer
   zaps you). Three keys wake the shapeshifting Guardian: a giant cat, then a swordsman, then a
   living hill of jelly, with a riddle between shapes (a wrong answer heals it).
9. **Another Dimension** – floating crystal islands over a sea of living energy, in light
   gravity. Catch five energy sprites that run from you, climb the crystal spire, and ride beams
   of light up to the sky island. Three motes of light open the temple and its ancient entity.

Then: *to be continued...* in chapter 10, The Laser Trap.

## How it's built

- `Source/SparkyLeo/Levels.cpp`: the four chapter worlds, built from blocks, cylinders and ramps
  into `ALevelGeo` (one mesh section per texture, with vertex lighting baked from lamps, N64
  style), plus the actors placed in them.
- `SparkyGameMode.cpp`: the title screen, story pages, dialog, and chapters 1–4's scripts; `SparkyGameModeChapters.cpp`: chapters 5–9's.
- `SparkyCharacter.cpp`: Sparky's moves, the camera and input. `Enemy.cpp`: bog blobs, alley cats,
  Meatball and the pirate dogs. `Props.cpp`: pickups, NPCs, Leo, moving platforms, velcro walls,
  hazards, checkpoints, goals, repair points, pods, the pirate ship and effects.
- `CreatureRig.cpp`: every character, built from rigid low-poly parts and animated in code.
- `SparkyHUD.cpp`: everything 2D, on a 320x240 virtual screen with a pixel font and pixel-art
  portraits. `Story.h` holds the story text.
- `Scripts/make_textures.py` and `Scripts/make_audio.py`: all textures, sound effects and the five
  music loops, generated by code.

Dev switches: `-SkipStory`, `-God`, `-Hat`, `-Spawn=x,y,z`, `-Wave=3` (chapter 2: straight to
Meatball; chapter 6: the Captain), `-Keys=3` (chapter 8: the Guardian), `-Berries=3` (chapter 5: the sneaking part), `-AutoPlay`, `-AutoClimb` / `-AutoWeld` (scripted tests), `-LogPos`, `-LogSfx`, `-KeyboardOnly`, `-AutoShot=<s>`,
`-FreeMouse`, `-NoMusic`. To screenshot-test without the game grabbing the mouse, add
`-FreeMouse -ini:Input:[/Script/Engine.InputSettings]:bCaptureMouseOnLaunch=False`.

## Next chapter

10 The Laser Trap: a space-station escape and a boss fight with Professor Whiskertron, Sparky's
old mentor from the Catnip Sector.
