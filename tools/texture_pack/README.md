# TR1 texture pack: room textures regenerated with Stable Diffusion

The game takes a texture pack from `texture/<LEVEL>_<tile>.jpg` beside its data, four times the size of the
tiles of the original. The engine keeps the colors and the shapes of the original and only takes the fine detail
of the pack (`blendHDTile` in `src/level.h`).

These scripts make the detail of the room textures (walls, floors, ceilings, waterfalls) anew with SD 1.5 img2img
and ControlNet tile, starting from a x4 upscale (Real-ESRGAN) of the tiles. Lara, the enemies, the items and the
static meshes keep the upscale: SD made up the shapes of their small textures.

1. `python roommask.py <TR1 data dir> roomrects.json`
   reads the PHD levels and writes the texel rects of the textures the rooms use (animated ones included).
2. `python regen.py <upscaled tiles dir> <out dir> [LEVEL_tile ...]`
   (with torch, diffusers, transformers, accelerate, pillow, numpy and a CUDA GPU; `roomrects.json` beside it)
   regenerates the tiles that have room textures and copies the others. `SD_STRENGTH` (0.45) sets how far it
   goes from the upscale, `SD_MASKS` takes other rect files. It skips the tiles already in the out dir.
3. Copy the out dir into `texture/` of the game.
