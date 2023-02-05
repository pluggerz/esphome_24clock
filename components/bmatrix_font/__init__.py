import os.path
from zipfile import ZipFile
from os import path
from esphome.core import CORE
from esphome.voluptuous_schema import _Schema
from esphome import core
from pathlib import Path
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import font
from esphome.const import (
    CONF_FILE,
    CONF_ID,
    CONF_SIZE,
    CONF_TYPE,
)

CONF_DAFONT = 'dafont'
DEPENDENCIES = ["font"]
CODEOWNERS = ["@hvandenesker"]
MULTI_CONF = True

DAFONT_SCHEMA = _Schema({
    cv.Required(CONF_ID): cv.declare_id(font.Font),
    cv.Required(CONF_FILE): str,
    cv.Required(CONF_SIZE): cv.int_range(min=1),
})

CONFIG_GROUP = cv.Any(
    cv.typed_schema(
        {
            CONF_DAFONT: cv.Schema(DAFONT_SCHEMA),
        }
    ),
)

CONFIG_SCHEMA = cv.All(CONFIG_GROUP)

def _compute_base_dir() -> Path:
    return  Path(CORE.config_dir) / ".esphome" / 'bmatrix_font'

def _compute_dir(name) -> Path:
            base_dir = _compute_base_dir()
            if not path.exists(base_dir):
                os.mkdir(base_dir)
            return base_dir / name

def provide_file(src, file):
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

def provide_dafont(name):
    zip_file=provide_file(f'https://dl.dafont.com/dl/?f={name}', f"{name}.zip")
    # loading the temp.zip and creating a zip object
    with ZipFile(zip_file, 'r') as zObject:
        # Extracting specific file in the zip
        # into a specific location.
        ttf_file = zObject.extract(f"{name}.ttf", path=_compute_base_dir())
        print(f"Extracted to {ttf_file}")
    zObject.close()
    return ttf_file

def to_code_dafont(config):
    file=config[CONF_FILE]
    return provide_dafont(file)


async def to_code(config):
    # actually a wrapper around font, could be merged maybe in the future
    if CONF_DAFONT == config[CONF_TYPE]:
        src=to_code_dafont(config)
    else:
        raise Exception(f"Not supported: {config[CONF_TYPE]}")
    
    config[CONF_FILE]=src
    base_name=str(config[CONF_ID])
    fontConfig = {
        CONF_ID: config[CONF_ID],
        CONF_FILE: str(src),
        font.CONF_SIZE: config[CONF_SIZE],
        font.CONF_RAW_DATA_ID: core.ID(base_name+"_prog_arr_", is_declaration=True, type=cg.uint8),
        font.CONF_RAW_GLYPH_ID: core.ID(
                base_name+"_data", is_declaration=True, type=font.GlyphData)
        }
 
    # delegate the work
    validated = font.CONFIG_SCHEMA(fontConfig)
    await font.to_code(validated)

