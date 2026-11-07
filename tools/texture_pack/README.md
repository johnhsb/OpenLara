# TR1 texture pack: room textures regenerated with Stable Diffusion

These scripts build a high resolution texture pack for the TR1 levels. The room textures (walls, floors, ceilings,
waterfalls) get realistic surface detail made by Stable Diffusion. Everything else (Lara, the enemies, the items,
the static meshes) keeps a plain x4 upscale.

## How the game uses a pack

The game looks for `texture/<LEVEL>_<tile>.jpg` (or `.png`, the PNG needs no GL) beside its `data` folder, for
example `texture/LEVEL3A_0.jpg` for the first tile of `data/LEVEL3A.PHD`.

- Every tile must have the same multiple of 256 as its size; these scripts make 1024x1024 (x4).
- The pack is read when a level loads, so after changing the files, reload the level or a save.
- The engine doesn't show the pack as is (`blendHDTile` in `src/level.h`). It keeps the colors and the shapes of the
  original, smoothly upscaled, and only adds the fine detail of the pack over them. So a regenerated texture
  can't move far from the original, and textures that go on from one face to the next still meet.
- The cut-outs (leaves, grates, splashes) keep the transparency of the original, so the transparent parts of a
  pack tile can hold any color.
- `OL_TEXTURE_PACK_MIX=0..1` (environment variable) sets how much of the detail of the pack shows (1 by default).

## The scripts

| Script | Needs | Does |
|---|---|---|
| `extract_tiles.py` | Python 3 only | Writes the tiles of every PHD level as 256x256 RGBA PNGs. |
| `roommask.py` | Python 3 only | Finds the texture rects the rooms use, per level and tile, and writes them to `roomrects.json`. |
| `regen.py` | torch, diffusers, CUDA GPU | Redraws the room textures of the x4 tiles with SD and copies the other tiles unchanged. |

## 1. Setup (Windows, NVIDIA GPU)

The run below took about 1 hour on an RTX 4060 Laptop GPU (8 GB), with Python 3.13 and torch 2.14 (CUDA 12.6).

```bat
python -m venv C:\Users\<you>\tools\sdtex\venv
C:\Users\<you>\tools\sdtex\venv\Scripts\python -m pip install torch torchvision --index-url https://download.pytorch.org/whl/cu126
C:\Users\<you>\tools\sdtex\venv\Scripts\python -m pip install diffusers transformers accelerate safetensors pillow numpy
```

Check the GPU:

```bat
C:\Users\<you>\tools\sdtex\venv\Scripts\python -c "import torch; print(torch.cuda.is_available(), torch.cuda.get_device_name(0))"
```

The models download on the first run of `regen.py` (about 3 GB, into the Hugging Face cache):

- `stable-diffusion-v1-5/stable-diffusion-v1-5` (fp16)
- `lllyasviel/control_v11f1e_sd15_tile`

For the x4 upscale, get `realesrgan-ncnn-vulkan` (Windows build, with the `realesrgan-x4plus` model) from
https://github.com/xinntao/Real-ESRGAN/releases. It runs on any Vulkan GPU.

## 2. Extract the original tiles

```bat
python extract_tiles.py <TR1>\data <TR1>\texture\original
```

This writes `<TR1>\texture\original\<LEVEL>_<tile>.png`: the 8-bit tiles with the palette of their level, with palette
index 0 transparent. The transparent texels take the color of the opaque texels next to them (still at alpha 0), so
the upscaler doesn't darken the edges of the cut-outs.

## 3. Upscale them x4

```bat
realesrgan-ncnn-vulkan.exe -i <TR1>\texture\original -o <TR1>\texture\esrgan -n realesrgan-x4plus -s 4 -f jpg
```

This makes `<TR1>\texture\esrgan\<LEVEL>_<tile>.jpg`, 1024x1024. That folder alone is already a working pack: copy
it into `<TR1>\texture\` to play with the plain upscale. Keep `esrgan` as it is. It is the input of the next step
and the way back.

## 4. Find the room textures

```bat
python roommask.py <TR1>\data C:\Users\<you>\tools\sdtex\roomrects.json
```

For every level, this lists the texel rects `[tile, x0, y0, x1, y1]` of the object textures used by the faces of the
rooms and by the animated textures. It prints one line per level: the tile count, the room textures, all the
textures. In TR1, the room textures fill the first tiles of a level (1 to 6 of them).

## 5. Regenerate the room textures

Put `regen.py` beside `roomrects.json` and run it with the Python of the venv:

```bat
cd C:\Users\<you>\tools\sdtex
venv\Scripts\python regen.py <TR1>\texture\esrgan out
```

For every tile:

- **A tile with room textures:**
  - It goes through SD 1.5 img2img with ControlNet tile at 1024x1024.
  - The prompt is a generic "photorealistic texture" prompt plus a material hint per level (the `HINT` table).
  - Only the room texture rects are taken from the SD result. The rest of the tile stays upscaled.
- **A tile with none:** it is copied as it is.

It prints `LEVEL3A_0 84 textures regenerated` or `LEVEL3A_6 kept (none to regenerate)`.

Options:

- **Only some tiles:** name them, for example `regen.py <esrgan> out LEVEL3A_0 LEVEL3A_1`.
- **Resume:** tiles already in the out folder are skipped. Stop the run at any time and start it again. To redo a
  tile, delete it from `out` first.
- **`SD_STRENGTH`** (0.45 by default): how far SD may go from the upscale.
  - 0.35 keeps the drawing of the original closer.
  - 0.55 is more realistic but makes up more.
- **`SD_MASKS`** (`roomrects.json` by default): rect files to take, comma separated.
- **Seeds:** each tile has a fixed seed, so a rerun with the same settings gives the same tile.

A tile takes about 30 seconds, about 1 hour for the 105 tiles of TR1 with room textures. The warnings that
`diffusion_pytorch_model.safetensors` is missing for the ControlNet ("defaulting to unsafe serialization") are
harmless: that model has only a `.bin`.

## 6. Install and compare

```bat
copy /Y C:\Users\<you>\tools\sdtex\out\*.jpg <TR1>\texture\
```

To switch between the plain upscale and the SD pack, two batch files in the game folder help:

```bat
rem tex_esrgan.bat: the plain x4 upscale
copy /Y "%~dp0texture\esrgan\*.jpg" "%~dp0texture\" >nul

rem tex_sd.bat: the SD room textures
copy /Y "C:\Users\<you>\tools\sdtex\out\*.jpg" "%~dp0texture\" >nul
```

To compare:

1. Save in the game.
2. Run one of the batch files.
3. Load the save, so the level and its pack load again.
4. Take a screenshot.
5. Repeat with the other batch file.

To check the plain upscale of the original without any pack, rename the `texture` folder for a while.

## Why only the room textures

Static meshes were tried, two ways:

- **Whole tiles:** SD blended the small textures packed next to each other into one another. Leaves broke into
  shards, and the bindings of a pole turned into leaves.
- **One texture at a time:** this fixed the leaves, but SD still changed what small detailed textures show. For
  example, it redrew the figures of a carved relief.

Characters and items have the same problem, worse. So they keep the plain upscale. The room textures are large,
repeating materials (rock, stone, wood, grass), where SD adds detail without changing what they are.
