# depending on https://materialdesignicons.com/
# 'brew install node' for np
# 'npm install @mdi/font' for the fonts
# 'npm install @mdi/util' for the util to process them
#  'npm install @mdi/svg' the actual glyphs
# https://www.cdnpkg.com/MaterialDesign-Webfont/file/materialdesignicons-webfont.ttf the font

from cgitb import text
from logging import Logger
import logging
import os.path
from svglib.svglib import svg2rlg
from os import defpath, path
from typing import Optional
from esphome.components.font import (
    CONF_RAW_DATA_ID,
    CONF_RAW_GLYPH_ID
)
from PIL import Image, ImageFilter, ImageOps
from esphome.core import CORE
import importlib
from esphome.cpp_generator import MockObj, Pvariable
from esphome.voluptuous_schema import _Schema
from esphome import core
from pathlib import Path
from reportlab.graphics import renderPDF, renderPM
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import loader
import sys
import json
from esphome.components import sensor, binary_sensor, switch, image, font, time, text_sensor
from esphome.const import (
    CONF_BINARY_SENSOR,
    CONF_DEVICE_CLASS,
    CONF_DISABLED_BY_DEFAULT,
    CONF_FILE,
    CONF_FORMAT,
    CONF_GLYPHS,
    CONF_ID,
    CONF_NAME_FONT,
    CONF_NAME,
    CONF_RANDOM,
    CONF_SENSOR,
    CONF_GROUP,
    CONF_SIZE,
    CONF_ICON,
    CONF_SOURCE,
    CONF_TYPE,
    CONF_VISIBLE,
    CONF_VALUE,
    DEVICE_CLASS_MOTION,
    DEVICE_CLASS_TEMPERATURE,
)

GLOBAL_MDI_KEY='c79818f3-e120-4fbd-b031-de64dc5bddc4'


CONF_DEFAULT_SIZE = "default_size"
CONF_SIZES = "sizes"
CONF_WIDGETS = "widgets"
CONF_MDI = "mdi"
CONF_TEXT = "text"
CONF_TEXT_SENSOR = "text_sensor"

class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

_LOGGER = logging.getLogger(__name__)


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
        
# based on https://stackoverflow.com/questions/6589358/convert-svg-to-png-in-python
import subprocess
import sys

def install(package):
    subprocess.check_call([sys.executable, "-m", "pip", "install", package])


DOMAIN = "svg"
import hashlib

def _compute_local_svg_dir(name) -> Path:
    base_dir = Path(CORE.config_dir) / ".esphome" / DOMAIN
    if not path.exists(base_dir):
        os.mkdir(base_dir)
    return base_dir / name
def download(icon):
    file=f"{icon}.svg"
    src=f"https://raw.githubusercontent.com/Templarian/MaterialDesign/master/svg/{file}"
    dst=_compute_local_svg_dir(file)
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



def decide(x,y,s,idx00, idx01, idx02,idx10, idx11, idx12,idx20, idx21, idx22):
        all= [idx00, idx01, idx02,idx10, idx11, idx12,idx20, idx21, idx22]
        blacks=all.count(0)
        whites=all.count(9)
        total_all=sum(all)
        max_idx=max(all)
        min_idx=min(all)
        if max_idx == min_idx:
            return ('=', max_idx)
        
        neighbors=[idx00, idx01, idx02,idx10, idx12,idx20, idx21, idx22]
        max_idx=max(all)
        min_idx=min(all)
        if max_idx == min_idx and idx11 != max_idx:
            return ('*', idx11)

        # follow drawn strokes
        if x -.5 <= s/2 and idx00 == 0 and idx10 == 0 and idx20 == 0 and total_all >= 6 * 9:
            return ('|', 0)
        if idx01 == 0 and idx11 == 0 and idx21 == 0 and total_all >= 4 * 9:
            return ('|', 0)
        if x +.5 >= s/2 and idx02 == 0 and idx12 == 0 and idx22 == 0 and total_all >= 6 * 9:
            return ('|', 0)
        
        if y <= s/2 and idx00 == 0 and idx01 == 0 and idx02 == 0 and total_all >= 6 * 9:
            return ('-', 0)
        if idx10 == 0 and idx11 == 0 and idx12 == 0 and total_all == 4 * 9:
            return ('-', 0)
        if y >= s/2 and idx20 == 0 and idx21 == 0 and idx22 == 0 and total_all >= 6 * 9:
            return ('-', 0)
        
        if idx00 == 0 and idx11 == 0 and idx22 == 0 and total_all >= 6 * 9:
            return ('\\', 0)

        if idx20 == 0 and idx11 == 0 and idx02 == 0 and total_all >= 6 * 9:
            return ('/', 0)

        # make sure white holes are prefered
        #if idx11 == 9:
        #    if whites <= 3:
        #        return ('h', 9)    
        weights=[1/4,1/2,1/4,1/2,1,1/2, 1/4, 1/2, 1/4]
        ret=round(sum([v*w for v,w in zip(all,weights)])/sum(weights))
        return ('?', ret)


def draw_image(alias, image):
        width = image.width
        height = image.height
        data= list(image.getdata(0))
        for y in range(height):
            line=""
            for x in range(width):
                idx = data[y * width + x] * 9 // 255
                if 1 == (x % 2) and 1 == (y % 2):
                    line+=bcolors.OKCYAN
                else:
                    line+=bcolors.OKBLUE
                if idx == 0 or idx == 1:
                    line+="█"
                elif idx==9 or idx == 8:
                    line+="_"
                else: 
                    line+=f'{idx}'
                line+=bcolors.ENDC
                
            print(f"{y:2d} {line}")

def draw_image2(alias, image, block_size):
        width = image.width
        height = image.height
        data= list(image.getdata(0))
        for y in range(height):
            line=""
            for x in range(width):
                idx = data[y * width + x] * 9 // 255
                if ((x//block_size) % 2) == ((y//block_size) % 2):
                    line+=bcolors.WARNING
                else:
                    line+=bcolors.OKGREEN
                if idx == 0 or idx == 1:
                    line+="█"
                elif idx==9 or idx == 8:
                    line+="_"
                else: 
                    line+=f'{idx}'
                line+=bcolors.ENDC
                
            print(f"{y:2d} {line}")


def use_cropping(src, requested_size):
    uncroppped_size=requested_size+2
    drawing = svg2rlg(src)
    scale = uncroppped_size / drawing.width
    drawing.scale(scale, scale)
    drawing.width *= scale
    drawing.height *= scale

    img = renderPM.drawToPILP(drawing).convert('1')
    # check if valid
    min_value=120
    for x in range(uncroppped_size):
        if img.getpixel((x,0)) < min_value:
            print(f"fail: ({x},0): {img.getpixel((x,0))}")
            return None
        if img.getpixel((0,x)) < min_value:
            print(f"fail: (0,{x}): {img.getpixel((0,x))}")
            return None
        if img.getpixel((x,uncroppped_size - 1)) < min_value:
            print(f"fail: ({x},{uncroppped_size - 1}): {img.getpixel((x,uncroppped_size - 1))}")
            return None
        if img.getpixel((uncroppped_size - 1,x)) < min_value:
            print(f"fail: ({uncroppped_size - 1}, {x}): {img.getpixel((uncroppped_size - 1,x))}")
            return None

    img = renderPM.drawToPILP(drawing).convert('L')
    print("uncropped")
    draw_image(src, img)
    return img.crop((1,1,1+requested_size,1+requested_size))


def transform1(src):
    requested_size=8
    
    drawing = svg2rlg(src)
    inbetween_size=2 * requested_size
    scale = inbetween_size / drawing.width
    drawing.scale(scale, scale)
    drawing.width *= scale
    drawing.height *= scale

    image1 = renderPM.drawToPILP(drawing).convert('1')
    draw_image2(src, image1, 2)

    scale = requested_size / drawing.width
    drawing.scale(scale, scale)
    drawing.width *= scale
    drawing.height *= scale
    image2 = renderPM.drawToPILP(drawing).convert('1')
    draw_image2(src, image2, 1)

    def getpixel(p):
        return 9*image1.getpixel(p) // 255

    final_image=Image.new('1', (requested_size, requested_size))
    for y in range(requested_size):
        v0=y*2
        v1=y*2+1
        for x in range(requested_size):
            u0=x*2
            u1=x*2+1
            p=(x,y)
            idx=image2.getpixel(p)

            n00=getpixel((u0,v0))
            n10=getpixel((u1,v0))
            n01=getpixel((u0,v1))
            n11=getpixel((u1,v1))
            
            raw=[00, n10, n01, n11]
            if idx == 255 and raw.count(0) >= 2:
                final_image.putpixel(p, 0)
            else:
                final_image.putpixel(p, idx//2)
    draw_image2(src, final_image, 1)
    


            

            

        

            




def transform1_cropping(src):
    requested_size=8
    image=use_cropping(src, requested_size)
    if image == None:
        print(f"Cropping failed for {src}")

def transform_edges(dst):
    requested_size=8
    small_size=(requested_size, requested_size)
    
    drawing = svg2rlg(dst)
    print(f"drawing: {drawing.width}x{drawing.height}")
    
    inbetween_size=2 * requested_size + 1
    #inbetween_size=requested_size
    scale = inbetween_size / drawing.width
    drawing.scale(scale, scale)
    drawing.width *= scale
    drawing.height *= scale

    pil_img = renderPM.drawToPILP(drawing).convert('L')
    total_image=Image.new(pil_img.mode, (3 * inbetween_size, inbetween_size))
    total_small_image=Image.new(pil_img.mode, (3 * requested_size, requested_size))
    total_image.paste(pil_img, (0,0))
    small_pil_img=pil_img.resize(small_size).filter(ImageFilter.EDGE_ENHANCE)
    total_small_image.paste(small_pil_img, (0,0))
    
    inner_edges = ImageOps.invert(ImageOps.invert(pil_img).filter(ImageFilter.FIND_EDGES))
    total_image.paste(inner_edges, (inbetween_size,0))


    expanded_image=ImageOps.expand(pil_img,border=inbetween_size,fill='white')
    outer_edges = ImageOps.invert(expanded_image.filter(ImageFilter.FIND_EDGES))
    outer_edges = outer_edges.crop((inbetween_size,inbetween_size,2*inbetween_size, 2*inbetween_size))
    total_image.paste(outer_edges, (2 * inbetween_size,0))

    draw_image(dst, total_image)
    
    small_inner_edges=inner_edges.resize(small_size)
    total_small_image.paste(small_inner_edges, (1 * requested_size,0))

    
    small_outer_edges=outer_edges.resize(small_size);
    total_small_image.paste(small_outer_edges, (2 * requested_size,0))

    draw_image(dst, total_small_image)
    
    def count_pixels(x, y, image):
        count=0
        for u in range(3):
            for v in range(3):
                ix = 2 * x + u
                iy = 2 * y + v
                pixel=image.getpixel((ix, iy))
                if pixel > 128:
                    count+=1
        return count

    def average_pixels(x, y, image):
        count=0
        pixels=0
        for u in range(3):
            for v in range(3):
                ix = 2 * x + u
                iy = 2 * y + v
                count+=image.getpixel((ix, iy))
                pixels+=1
        return 9 * (count // pixels) // 255

    def predict(x,y,image):
        average=average_pixels(x,y,image)
        if average ==0 or average == 9:
            return 'A', average

        inner=9 * small_inner_edges.getpixel((x,y)) // 255
        outer=9 * small_outer_edges.getpixel((x,y)) // 255
        
        if inner < outer:
            return 'i', 0
        
        
        #if inner > 3 and outer > 3:
        #    if inner >= outer:
        #        return 'i', 9
        #    else:
        #        return 'o', 0
        return 'a', average


    print("predict:")
    for y in range(requested_size):
        line=""
        w_line=""
        for x in range(requested_size):
            what, value = predict(x, y, pil_img)
            w_line+=what
            if value <= 2:
                value=0
            elif value >= 8:
                value=9
            # value = 9 * value // 255
            if value == 0:
                line+="█"
            elif value==9:
                line+="_"
            else: 
                line+=f'{value}'
        print(f"{y:2d} {line} {w_line}")






    

            

def transform1_sharpen(dst):
    requested_size=8
    
    drawing = svg2rlg(dst)
    print(f"drawing: {drawing.width}x{drawing.height}")
    
    inbetween_size=2 * requested_size + 1
    #inbetween_size=requested_size
    scale = inbetween_size / drawing.width
    drawing.scale(scale, scale)
    drawing.width *= scale
    drawing.height *= scale

    pil_img = renderPM.drawToPILP(drawing).convert('L')
    width = pil_img.width
    height = pil_img.height
    print(f"pil_img: {pil_img.width}x{pil_img.height}")



    print("in_between:")
    draw_image(dst, pil_img)

    def as_requested(dst, image):
        resized=image.resize((requested_size, requested_size), resample=Image.LANCZOS)
        draw_image(dst, resized)
        draw_image(dst, resized.filter(ImageFilter.SHARPEN) )
        

    # Detecting Edges on the Image using the argument ImageFilter.FIND_EDGES
    sharpen_img=pil_img
    for i in range(3):
        sharpen_img = sharpen_img.filter(ImageFilter.SHARPEN)  
        print("sharpen:")
        draw_image(dst, sharpen_img)   

        as_requested(dst, sharpen_img)   
           



def transform2(dst):
    requested_size=8
    
    drawing = svg2rlg(dst)
    print(f"drawing: {drawing.width}x{drawing.height}")
    
    inbetween_size=2 * requested_size + 1
    scale = (2 * requested_size + 1) / drawing.width
    drawing.scale(scale, scale)
    drawing.width *= scale
    drawing.height *= scale

    pil_img = renderPM.drawToPILP(drawing)
    width = pil_img.width
    height = pil_img.height
    print(f"pil_img: {pil_img.width}x{pil_img.height}")

    if width != height:
        raise Exception(f"Expected the same size!! got: {width}x{height}")
    if width != inbetween_size:
        raise Exception(f"Expected the same size!! got: {width}x{height}")
    
    pil_img_l=pil_img.convert("1")
    data= list(pil_img_l.getdata(0))

    print(f"{dst} in between")
    for y in range(inbetween_size):
        line=""
        for x in range(inbetween_size):
            pos = y  * inbetween_size  + x
            idx=data[pos] * 9 // 255
            #if idx == 1:
            #    idx=0
            #elif idx == 8:
            #    idx=9
            data[pos]=idx
            if 1 == (x % 2) and 1 == (y % 2):
                line+=bcolors.OKCYAN
            else:
                line+=bcolors.OKBLUE
            if idx == 0:
                line+="█"
            elif idx==9:
                line+="_"
            else: 
                line+=f'{idx}'

            line+=bcolors.ENDC
        print(f"{dst} {y:2d}: {line}")
    sampled_data=[None] * requested_size * requested_size

    print(f"{dst} in small")
    for y in range(requested_size):
        line=""
        op_line=""
        for x in range(requested_size):
            x0 = 2*x + 0
            x1 = 2*x + 1
            x2 = 2*x + 2

            y0 = (2*y + 0) * inbetween_size
            y1 = (2*y + 1) * inbetween_size
            y2 = (2*y + 2) * inbetween_size

            idx00=data[y0+x0]
            idx01=data[y0+x1]
            idx02=data[y0+x2]
            
            idx10=data[y1+x0]
            idx11=data[y1+x1]
            idx12=data[y1+x2]
            
            idx20=data[y2+x0]
            idx21=data[y2+x1]
            idx22=data[y2+x2]
       
            op, idx= decide(x,y,requested_size, idx00, idx01, idx02,idx10, idx11, idx12,idx20, idx21, idx22)
            if idx >= 7:
                idx=9
            elif idx <= 3:
                idx=0
                
            sampled_data[x + y*requested_size] = idx
            
            if idx == None:
                line+="?"
            elif idx == 0:
                line+="█"
            elif idx==9:
                line+="_"
            else: 
                line+=f'{idx}'
            op_line+=op
        
        last_value=9
        for x in range(requested_size-2):
            def get_value(offset):
                return sampled_data[y * requested_size + x + offset]
            v0=get_value(0)
            v1=get_value(1)
            v2=get_value(2)
            if v1 > max(v0, v2):
                idx=9
            elif v1 < min(v0, v2):
                idx=0
            else:
                idx=None
            if idx:
                sampled_data[y * requested_size + x + 1]=idx
            

        cline=""                
        for x in range(requested_size):
            idx=sampled_data[x + y*requested_size]
            if idx == 0:
                cline+="█"
            elif idx==9:
                cline+="_"
            else: 
                cline+=f'{idx}'
                
            
        print(f"{dst} {2*y+1:2d}: {line} {op_line} {cline}")

    
    for x in range(requested_size):
         for y in range(requested_size-2):
            def get_value(offset):
                return sampled_data[((y+offset) * requested_size) + x]
            v0=get_value(0)
            v1=get_value(1)
            v2=get_value(2)
            if v1 > max(v0, v2):
                idx=9
            elif v1 < min(v0, v2):
                idx=0
            else:
                idx=None
            if idx:
                sampled_data[((y+1) * requested_size) + x]=idx
    for x in range(requested_size):
         for y in range(requested_size):
            idx = sampled_data[y * requested_size  + x]
            if idx < 9:
                sampled_data[y * requested_size  + x]=0
    
    print("Final:")
    for y in range(requested_size):
        line=""
        for x in range(requested_size):
            idx=sampled_data[x + y*requested_size]
            if idx == 0:
                line+="█"
            elif idx==9:
                line+="_"
            else: 
                line+=f'{idx}'
        print(f"{dst} {2*y+1:2d}: {line}")

    
    

        



def transform3(dst):
    requested_size=8
    
    drawing = svg2rlg(dst)
    print(f"drawing: {drawing.width}x{drawing.height}")
    
    scale = 3 * requested_size / drawing.width
    drawing.scale(scale, scale)
    drawing.width *= scale
    drawing.height *= scale

    pil_img = renderPM.drawToPILP(drawing)
    width = pil_img.width
    height = pil_img.height
    if width != height:
        raise Exception(f"Expected the same size!! got: {drawing.width}x{drawing.height}")
    
    print(f"{pil_img} -> {width}x{height}")

    pil_img_l=pil_img.convert("L")
    pil_img_1=pil_img.convert("1")
    
    data= list(pil_img_l.getdata(0))
    
    for y in range(3 * requested_size):
        if (y % 3) == 0:
            print(" ")

        line=""
        for x in range(3 * requested_size):
            if (x % 3) == 0:
                line+=" "; 

            pos = y  * 3 * requested_size  + x
            idx=data[pos] * 9 // 255
            if idx == 8:
                idx=9
            elif idx == 1:
                idx=0
            data[pos]=idx
            if idx == 0:
                line+="█"
            elif idx==9:
                line+="_"
            else: 
                line+=f'{idx}'
        print(f"{dst} {y:2d}: {line}")

    print(f"{dst} small")
    def decide(idx00, idx01, idx02,idx10, idx11, idx12,idx20, idx21, idx22):
        total=sum([idx00, idx01, idx02,idx10, idx11, idx12,idx20, idx21, idx22])
        line0=sum([idx20,idx11,idx02])
        line1=sum([idx00,idx11,idx22])
        line2=sum([idx10,idx11,idx12])
        line3=sum([idx01,idx11,idx20])



        raw=[idx00, idx01, idx02,idx10, idx12,idx20, idx21, idx22]
        average=sum(raw) // 8
        max_idx=max(raw)
        min_idx=min(raw)
        if average > idx11 + 2:
            return 0
        if average < idx11 - 2:
            return 9
            
        raw=[idx00, idx01, idx02,idx10, idx11, idx12,idx20, idx21, idx22]
        max_idx=max(raw)
        min_idx=min(raw)
        if max_idx < 2:
            return 0
        if min_idx > 7:
            return 9
        blacks=sum(1 for i in raw if i < 2)
        whites=sum(1 for i in raw if i > 7)
        if whites == 9 or whites == 8:
            return 9
        if blacks == 9 or blacks == 8:
            return 0

        average = (sum(raw)) // 9
        return average


    sampled_data=[None] * requested_size * requested_size


    def decide2(idx00, idx01, idx02,idx10, idx11, idx12,idx20, idx21, idx22):
        raw=[idx00, idx01, idx02,idx10, idx11, idx12,idx20, idx21, idx22]

        blacks=sum(1 for i in raw if i == 0)
        whites=sum(1 for i in raw if i == 9)
        if whites > 0 and blacks == 0:
            return 9
        if blacks > 0 and whites == 0:
            return 0

        return sum(raw) // len(raw)

    for y in range(requested_size):
        line=""
        for x in range(requested_size):
            x0 = 3*x + 0
            x1 = 3*x + 1
            x2 = 3*x + 2

            y0 = (3*y + 0) * 3 * requested_size
            y1 = (3*y + 1) * 3 * requested_size
            y2 = (3*y + 2) * 3 * requested_size

            idx00=data[y0+x0]
            idx01=data[y0+x1]
            idx02=data[y0+x2]
            
            idx10=data[y1+x0]
            idx11=data[y1+x1]
            idx12=data[y1+x2]
            
            idx20=data[y2+x0]
            idx21=data[y2+x1]
            idx22=data[y2+x2]
            
            idx=decide2(idx00, idx01, idx02,idx10, idx11, idx12,idx20, idx21, idx22)
            sampled_data[x + y*requested_size] = idx

            if idx == None:
                line+="?"
            elif idx == 0:
                line+="█"
            elif idx==9:
                line+="_"
            else: 
                line+=f'{idx}'
        print(f"{dst} {y:2d}: {line}")

    
        
    


            
        
            
            



    #raise Exception("Please check it out")
        
        
    
async def to_code(config):
    install('pymupdf')
    install('svglib')

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

    MDI_META_FILE = find_file("../icons/icons.json")
    MDI_TTF = find_file('../icons/materialdesignicons-webfont.ttf')

    with open(MDI_META_FILE) as metaJson:
        MDI_MAP = dict((i['name'], chr(int(i['codepoint'], 16)))
                   for i in json.load(metaJson))
    # check if we are complete
    for icon in MDI_SELECTED_GLYPHS_NAMES:
        dst=download(icon)
        transform1(dst)
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
 
        # delegete the work
        validated = font.CONFIG_SCHEMA(fontConfig)
        await font.to_code(validated)
        await to_screen(size, reversed_mapping, validated)
        
        cg.add(var.register_font(size, await cg.get_variable(core.ID(fontId, is_declaration=False, type=font.Font))))

       


