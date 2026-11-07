# regenerates the room textures of a TR1 texture pack with SD 1.5 img2img + ControlNet tile
# usage: regen.py <texture dir> <out dir> [tile names...]   (the input is <texture dir>/<LEVEL>_<n>.jpg, x4 upscales)
import sys, os, json, glob
import numpy as np, torch
from PIL import Image
from diffusers import StableDiffusionControlNetImg2ImgPipeline, ControlNetModel

HINT = {
    'GYM': 'english manor interior, wood panels, plaster, marble',
    'LEVEL1': 'cave rock, ice, wooden bridge, snow, stone', 'LEVEL2': 'ancient inca stone ruins, mossy stone blocks, grass, wood',
    'LEVEL3A': 'jungle valley rock cliff, grass, moss, waterfall, wood', 'LEVEL3B': 'ancient inca tomb, carved stone, gold',
    'LEVEL4': 'greek stone temple, marble, metal, carved stone', 'LEVEL5': 'roman colosseum, sandstone, marble, rusty metal',
    'LEVEL6': 'greek palace, marble, gold, stone', 'LEVEL7A': 'flooded cistern, wet mossy stone, rusty metal',
    'LEVEL7B': 'egyptian tomb, carved stone, marble', 'LEVEL8A': 'egyptian city, sandstone, hieroglyphs, carved stone',
    'LEVEL8B': 'egyptian obelisk temple, sandstone, hieroglyphs, gold', 'LEVEL8C': 'egyptian sphinx sanctuary, sandstone, water',
    'LEVEL10A': 'mine, rock, wooden beams, rusty metal', 'LEVEL10B': 'atlantis, organic flesh walls, alien stone',
    'LEVEL10C': 'great pyramid, organic flesh, alien stone, lava', 'CAT': 'egyptian catacombs, sandstone, bones',
    'EGYPT': 'egyptian temple, sandstone, hieroglyphs', 'END': 'atlantis, organic flesh walls, alien stone', 'END2': 'stone, rock',
}
POS = 'photorealistic high resolution game texture, %s, sharp natural surface detail, physically based material, intricate, 8k'
NEG = 'blurry, smooth, painting, cartoon, text, watermark, logo, people, face, lowres, jpeg artifacts, oversaturated'

tex, outdir = sys.argv[1], sys.argv[2]
names = sys.argv[3:] or sorted(os.path.basename(p)[:-4] for p in glob.glob(os.path.join(tex, '*.jpg')))
rects = {}
for m in os.environ.get('SD_MASKS', 'roomrects.json').split(','):   # the textures to regenerate: of the rooms, the static meshes
    for level, rs in json.load(open(os.path.join(os.path.dirname(__file__), m))).items():
        rects.setdefault(level, []).extend(rs)
strength = float(os.environ.get('SD_STRENGTH', '0.45'))
os.makedirs(outdir, exist_ok=True)

cn = ControlNetModel.from_pretrained('lllyasviel/control_v11f1e_sd15_tile', torch_dtype=torch.float16)
pipe = StableDiffusionControlNetImg2ImgPipeline.from_pretrained('stable-diffusion-v1-5/stable-diffusion-v1-5', controlnet=cn,
    torch_dtype=torch.float16, variant='fp16', safety_checker=None, requires_safety_checker=False).to('cuda')
pipe.vae.enable_tiling()
pipe.set_progress_bar_config(disable=True)

for name in names:
    dst = os.path.join(outdir, name + '.jpg')
    if os.path.exists(dst): continue
    level, tile = name.rsplit('_', 1); tile = int(tile)
    rs = [r for r in rects.get(level, []) if r[0] == tile]
    src = Image.open(os.path.join(tex, name + '.jpg')).convert('RGB')
    if not rs:
        src.save(dst, quality=95); print(name, 'kept (none to regenerate)', flush=True); continue
    g = torch.Generator('cuda').manual_seed(tile * 7919 + sum(map(ord, level)))
    ai = pipe(prompt=POS % HINT.get(level, 'stone, rock, wood, metal'), negative_prompt=NEG, image=src, control_image=src,
              strength=strength, num_inference_steps=30, guidance_scale=6.0, controlnet_conditioning_scale=1.0, generator=g).images[0]
    s = src.width // 256
    a = np.array(src); b = np.array(ai.resize(src.size, Image.LANCZOS))
    for _, x0, y0, x1, y1 in rs:   # only the textures picked, the others (Lara, enemies, items) keep the upscale
        a[y0*s:y1*s, x0*s:x1*s] = b[y0*s:y1*s, x0*s:x1*s]
    Image.fromarray(a).save(dst, quality=95)
    print(name, len(rs), 'textures regenerated', flush=True)
