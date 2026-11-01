# Editor settings

![editor settings](preview-settings.png)

`ENTER` (PSP: `SELECT`) opens the settings window, `Z` / `ESC` closes it.
Up / down walks the rows, `X` (or a click) does what the row says. Every row
shows what it will do on its right edge: `BROWSE`, `TYPE`, `PICK`.

| row | what it holds |
| --- | --- |
| `WAD FILE (TEXTURES)` | the `.wad` the block textures come from |
| `COMPILED FILES GO TO` | folder for `kabluk.map`, `.bsp`, `.prt` and the logs |
| `MAP SOURCE NAME (.MAP,.BSP)` | the name of the map, without the extension |
| `FLOOR TEXTURE` | texture of the floor slab the exporter writes |
| `CEILING TEXTURE` | texture of the ceiling slab |
| `SAVE .MAP` | writes **only** the `.map` source to that folder |
| `MAIN MENU` | shows the start screen; the map stays loaded, circle goes back to it |
| `CLOSE` | back to the editor |

## The map name and the folder

The exporter writes `<folder>/<name>.map`; qbsp then puts `<name>.bsp`,
`<name>.prt` and `<name>.log` next to it. `COMPILE MAP` (F2 window) uses the
same pair of settings, so a compile and a save always agree on the path.

The name is sanitised before it reaches the filesystem: path separators,
`:` and anything Windows rejects become `_`, trailing dots and spaces are
dropped (`my/room` → `my_room`). An empty field falls back to `kabluk`.
A missing output folder is created when the map is written.

`SAVE .MAP` is the "save the source" button: it runs the exporter and stops
before `qbsp`/`light`/`vis`, so the text `.map` is on disk in a moment and the
result line under the buttons shows the file it wrote:

```
MAPS/MYROOM.MAP
```

## Floor and ceiling textures

Both slabs are chosen from the loaded `.wad` with the same list the `TEXTURE`
tool uses — names and thumbnails, up / down / `X`. An empty row means "first
texture in the wad", which is what every map used before the setting existed.

The same floor texture is used for the sealing walls the exporter adds around
the border of the map (see `compile-chain.md`), so a map built without any
wall of its own still looks finished.

## The file browser can create folders

![file browser](preview-browser.png)

`NEW FOLDER` (or `N`) asks for a name on the on screen keyboard
(`docs/properties.md` has the keys) and creates it inside the folder that is
open. The list, the entry counter and the path field refresh right away, so
the new folder can be entered or picked as the output folder immediately.

The browser is used for three things: the `.wad`, the output folder
(`( USE THIS FOLDER )` confirms the folder you are in) and nothing else — it
never deletes or renames anything.

On the PSP the output folder browser starts on `ms0:/`, the root of the
memory stick: the editor itself runs from `ms0:/PSP/GAME/KABLUK` and every
relative path in the settings lives there, but `.. ( UP )` climbs out of the
EBOOT folder, all the way to `ms0:/`, where it stops. Once a folder has been
picked it is opened again next time, and `( USE THIS FOLDER )` stores its full
path (`ms0:/PSP/GAME/MYMAPS`), so a card can be laid out the way its owner
wants instead of being tied to the EBOOT folder.

## What is written into the `.map`

The exporter remembers the `.wad` in the worldspawn entity:

```
"wad" "./tex.wad"
```

`qbsp` would look for that file next to the `.map`, which is not where the
editor keeps it, so the compile chain also passes `-wadpath <that folder>`.
Without it every texture came back as `Texture ... not found` and the `.bsp`
shipped without a single miptex; now the level carries the textures it shows
in the editor.
