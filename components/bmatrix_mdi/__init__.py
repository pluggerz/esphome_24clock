# depending on https://materialdesignicons.com/
# 'brew install node' for np
# 'npm install @mdi/font' for the fonts
# 'npm install @mdi/util' for the util to process them
#  'npm install @mdi/svg' the actual glyphs
# use https://www.npmjs.com/package/@mdi/font-build?activeTab=readme
# https://www.cdnpkg.com/MaterialDesign-Webfont/file/materialdesignicons-webfont.ttf the font

import os.path
from os import path
from esphome.components.font import (
    CONF_RAW_DATA_ID,
    CONF_RAW_GLYPH_ID
)
import importlib
from esphome.voluptuous_schema import _Schema
from esphome import core
from esphome.core import CORE
from pathlib import Path
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import loader
import sys
import json
from esphome.components import font
from esphome.const import (
    CONF_FILE,
    CONF_GLYPHS,
    CONF_ID,
    CONF_SIZE,
    CONF_ICON,
    CONF_TYPE,
)

GLOBAL_MDI_KEY='c79818f3-e120-4fbd-b031-de64dc5bddc4'


CONF_DEFAULT_SIZE = "default_size"
CONF_SIZES = "sizes"
CONF_WIDGETS = "widgets"
CONF_MDI = "mdi"
CONF_TEXT = "text"
CONF_TEXT_SENSOR = "text_sensor"

DEPENDENCIES = ["display"]
CODEOWNERS = ["@hvandenesker"]
MULTI_CONF = False

bmatrix_ns = cg.esphome_ns.namespace("bmatrix")
Mdi = bmatrix_ns.class_("Mdi")

MDI_SELECTED_GLYPHS_NAMES = []

CONFIG_SCHEMA = _Schema(
    {
        cv.Optional(CONF_ID, 'default_mdi'): cv.declare_id(Mdi),
        cv.Required(CONF_SIZES): cv.ensure_list(int),
        cv.Required(CONF_DEFAULT_SIZE): int,
        cv.Required(CONF_ICON): cv.ensure_list(str),
    }
)

def find_file(file: str) -> Path:
    for p in sys.meta_path:
        print(f"possible meta_path: {p}")
        if (isinstance(p, loader.ComponentMetaFinder)):
            for f in p._finders:
                if (isinstance(f, importlib.machinery.FileFinder)):
                    print(f"possible finder: {f}")
                    path = Path(str(f.path)) / str(file)
                    print(f"path: {path}")
                    if (Path(path).exists()):
                        return path
    raise cv.Invalid(f"Not found: {file}")


async def to_screen(size, reversed_mapping, config):
    conf = config[CONF_FILE]
    if conf[CONF_TYPE] == font.TYPE_LOCAL_BITMAP:
        loaded_font = font.load_bitmap_font(CORE.relative_config_path(conf[font.CONF_PATH]))
    elif conf[CONF_TYPE] == font.TYPE_LOCAL:
        path = CORE.relative_config_path(conf[font.CONF_PATH])
        loaded_font = font.load_ttf_font(path, config[CONF_SIZE])
    elif conf[CONF_TYPE] == font.TYPE_GFONTS:
        path = font._compute_gfonts_local_path(conf)
        loaded_font = font.load_ttf_font(path, config[CONF_SIZE])
    else:
        raise core.EsphomeError(f"Could not load font: unknown type: {conf[CONF_TYPE]}")

    for glyph in config[CONF_GLYPHS]:
        any=False
        mask = loaded_font.getmask(glyph, mode="1")
        width, height = mask.size
        offset_x, offset_y = loaded_font.getoffset(glyph)
        print(f"----> size: {size}, bounds=({width},{height}), offset=({offset_x},{offset_y}) glyph={reversed_mapping[glyph]}")
        for y in range(offset_y):
            print(f"{y:2d}:@")
        for y in range(height):
            line_as_string = f"{y+offset_y:2d}:"
            for x in range(offset_x):
                    line_as_string += "@"
            
            for x in range(width):
                if  mask.getpixel((x, y)):
                    line_as_string += "█"
                    any=True
                else:
                    line_as_string += "_"
            print(line_as_string)
        print("<----")
        if not any:
            raise Exception(f"Glyph '{reversed_mapping[glyph]}' does not contain data !?")
        

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    
    if GLOBAL_MDI_KEY not in CORE.data:
        CORE.data[GLOBAL_MDI_KEY]={}
    known_mappings=CORE.data[GLOBAL_MDI_KEY]

    MDI_SELECTED_GLYPHS_NAMES = []
    id = config[CONF_ID]
    print(f"id: {id}")
    for icon in config[CONF_ICON]:
        if icon not in MDI_SELECTED_GLYPHS_NAMES:
            MDI_SELECTED_GLYPHS_NAMES.append(icon)
    for icon in [
        'account',
        'plus',
        'lock',
        'toggle-switch',
        'toggle-switch-off', 
        'help', 
        'bell', 
        'bell-ring', 
        'doorbell',
        'pencil',
        'coffee', 
        'walk', 
        'run',
        'filmstrip',
        ]:
        if icon not in MDI_SELECTED_GLYPHS_NAMES:
            MDI_SELECTED_GLYPHS_NAMES.append(icon)

    if len(MDI_SELECTED_GLYPHS_NAMES) == 0:
        return;


    def provide_file(src, file):
        def _compute_dir(name) -> Path:
            base_dir = Path(CORE.config_dir) / ".esphome" / 'mdi'
            if not path.exists(base_dir):
                os.mkdir(base_dir)
            return base_dir / name
        dst=_compute_dir(file)
        if not path.exists(dst):
            print(f'download: {src} -> {dst}')

            import requests
            r=requests.get(src)
        
            if not r.ok:
                raise Exception(f'Failed to download: {src}')
            with open(dst, 'wb') as f:
                f.write(r.content)
        else:
            print(f'Already available: {dst}')
        return dst

    MDI_META_FILE = provide_file('https://raw.githubusercontent.com/Templarian/MaterialDesign/master/meta.json', 'meta.json')
    MDI_TTF = provide_file('https://github.com/Templarian/MaterialDesign-Webfont/raw/master/fonts/materialdesignicons-webfont.ttf', 'webfont.ttf')
    
    with open(MDI_META_FILE) as metaJson:
        MDI_MAP = dict((i['name'], chr(int(i['codepoint'], 16)))
                   for i in json.load(metaJson))
    # check if we are complete
    for icon in MDI_SELECTED_GLYPHS_NAMES:
        if icon not in MDI_MAP:
            raise cv.Invalid(f"Icon not mapped: {icon}")

    print(f"MDI_SELECTED_GLYPHS_NAMES={MDI_SELECTED_GLYPHS_NAMES}")
    MDI_SELECTED_GLYPHS = set(MDI_MAP[name]
                              for name in MDI_SELECTED_GLYPHS_NAMES)
    print(f"MDI_SELECTED_GLYPHS={MDI_SELECTED_GLYPHS}")


    reversed_mapping={}
    for icon in MDI_SELECTED_GLYPHS_NAMES:
        code=MDI_MAP[icon]
        known_mappings[icon]=code
        reversed_mapping[code]=icon
        cg.add(var.register_glyph_mapping(icon, code))

    default_size= config[CONF_DEFAULT_SIZE]
    all_sizes={ default_size }
    cg.add(var.set_default_size(default_size))


    all_sizes.update(config[CONF_SIZES])
    for size in all_sizes:
        base_name = f"mdi_font_{id}_{size}"
        fontId = f"{base_name}"
        fontConfig = {
            CONF_ID: fontId,
            CONF_FILE: str(MDI_TTF),
            CONF_SIZE: size,
            CONF_GLYPHS: MDI_SELECTED_GLYPHS,
            CONF_RAW_DATA_ID: core.ID(base_name+"_prog_arr_", is_declaration=True, type=cg.uint8),
            CONF_RAW_GLYPH_ID: core.ID(
                base_name+"_data", is_declaration=True, type=font.GlyphData)
        }
 
        # delegate the work
        validated = font.CONFIG_SCHEMA(fontConfig)
        await font.to_code(validated)
        await to_screen(size, reversed_mapping, validated)
        
        cg.add(var.register_font(size, await cg.get_variable(core.ID(fontId, is_declaration=False, type=font.Font))))

       


