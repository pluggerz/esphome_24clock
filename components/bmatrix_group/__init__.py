# depending on https://materialdesignicons.com/
# 'brew install node' for np
# 'npm install @mdi/font' for the fonts
# 'npm install @mdi/util' for the util to process them
#  'npm install @mdi/svg' the actual glyphs
# https://www.cdnpkg.com/MaterialDesign-Webfont/file/materialdesignicons-webfont.ttf the font

from cgitb import text
from logging import Logger
import logging
from os import defpath
from typing import Optional
from esphome.components.font import (
    CONF_RAW_DATA_ID,
    CONF_RAW_GLYPH_ID
)
from esphome.core import CORE
import importlib
from esphome import automation
from esphome.automation import LambdaAction, maybe_simple_id
from esphome.cpp_generator import MockObj, Pvariable
from esphome.voluptuous_schema import _Schema
from esphome import core
from pathlib import Path
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
    CONF_SOURCE,
    CONF_TYPE,
    CONF_VISIBLE,
    CONF_VALUE,
    CONF_TIME,
    DEVICE_CLASS_MOTION,
    DEVICE_CLASS_TEMPERATURE,
)

CONF_WIDGETS = "widgets"
CONF_MDI = "mdi"
CONF_TEXT = "text"
CONF_TEXT_SENSOR = "text_sensor"
CONF_SWITCH = "switch"
CONF_ALERT_AGO = "alert_ago"

_LOGGER = logging.getLogger(__name__)



DEPENDENCIES = ["display"]
CODEOWNERS = ["@hvandenesker"]
MULTI_CONF = True

bmatrix_ns = cg.esphome_ns.namespace("bmatrix")

AlertAgoWidget = bmatrix_ns.class_('AlertAgoWidget')
BinarySensorWidget = bmatrix_ns.class_("BinarySensorWidget")  
DigitalTimeWidget = bmatrix_ns.class_("DigitalTimeWidget") 
MdiWidget = bmatrix_ns.class_("MdiWidget") 
SwitchWidget = bmatrix_ns.class_("SwitchWidget") 
TextWidget = bmatrix_ns.class_("TextWidget") 
TextSensorWidget = bmatrix_ns.class_("TextSensorWidget") 

AlertOnAction = bmatrix_ns.class_("AlertOnAction", automation.Action)
AlertOffAction = bmatrix_ns.class_("AlertOffAction", automation.Action)

Group = bmatrix_ns.class_("Group", cg.Component)
BMatrix = bmatrix_ns.class_("BMatrix", cg.Component)



WIDGET_ALERT_AGO_SCHEMA = _Schema({
    cv.GenerateID(CONF_RANDOM): cv.declare_id(AlertAgoWidget),
    cv.Optional(CONF_VALUE, '%s'): str
})

WIDGET_GROUP_SCHEMA = _Schema({
    cv.GenerateID(CONF_RANDOM): cv.declare_id(Group),
    cv.Required(CONF_VALUE): cv.use_id(Group)
})


WIDGET_MDI_SCHEMA = _Schema({
    cv.GenerateID(CONF_RANDOM): cv.declare_id(MdiWidget),
    cv.Required(CONF_VALUE): cv.string
})

WIDGET_SWITCH_SCHEMA = _Schema({
    cv.GenerateID(CONF_RANDOM): cv.declare_id(SwitchWidget),
    cv.Required(CONF_VALUE): cv.use_id(switch.Switch),
})

WIDGET_BINARY_SENSOR_SCHEMA = _Schema({
    cv.GenerateID(CONF_RANDOM): cv.declare_id(BinarySensorWidget),
    cv.Required(CONF_VALUE): cv.use_id(binary_sensor.BinarySensor),
})


WIDGET_TEXT_SCHEMA = _Schema({
    cv.GenerateID(CONF_RANDOM): cv.declare_id(TextWidget),
    cv.Required(CONF_VALUE): cv.string
})

WIDGET_TEXT_SENSOR_SCHEMA = _Schema({
    cv.GenerateID(CONF_RANDOM): cv.declare_id(TextSensorWidget),
    cv.Required(CONF_VALUE): cv.use_id(text_sensor.TextSensor)
})

WIDGET_TIME_SCHEMA = _Schema({
    cv.GenerateID(CONF_RANDOM): cv.declare_id(DigitalTimeWidget),
    cv.Required(CONF_VALUE): cv.use_id(time.RealTimeClock),
    cv.Optional(CONF_FORMAT, '%H:%M'): str
})



WIDGET_TYPE_SCHEMA = cv.Any(
    cv.typed_schema(
        {
            CONF_GROUP: cv.Schema(WIDGET_GROUP_SCHEMA),
            CONF_MDI: cv.Schema(WIDGET_MDI_SCHEMA),
            CONF_SWITCH: cv.Schema(WIDGET_SWITCH_SCHEMA),
            CONF_TEXT: cv.Schema(WIDGET_TEXT_SCHEMA),
            CONF_TEXT_SENSOR: cv.Schema(WIDGET_TEXT_SENSOR_SCHEMA),
            CONF_TIME: cv.Schema(WIDGET_TIME_SCHEMA),
            CONF_BINARY_SENSOR: cv.Schema(WIDGET_BINARY_SENSOR_SCHEMA),
            CONF_ALERT_AGO: cv.Schema(WIDGET_ALERT_AGO_SCHEMA),
        }
    ),
)

CONFIG_GROUP = _Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(Group),
        cv.Optional(CONF_NAME_FONT): cv.use_id(font.Font),
        cv.Required(CONF_WIDGETS): cv.ensure_list(WIDGET_TYPE_SCHEMA)
    }
)

CONFIG_SCHEMA = cv.All(CONFIG_GROUP)

async def to_code_widget_where_value_is_plain(idx, config):
    var = cg.new_Pvariable(config[CONF_RANDOM], config[CONF_VALUE])
    return var

async def to_code_widget_dummy(idx, config):
    print(f"{idx}: {config}");    
    var = cg.new_Pvariable(config[CONF_RANDOM])
    return var

async def to_code_widget_group(idx, config):  
    return await cg.get_variable(config[CONF_VALUE])

async def to_code_widget_mdi(idx, config):  
    known_mappings=CORE.data[GLOBAL_MDI_KEY]
    icon=config[CONF_VALUE]
    if icon not in known_mappings:
        raise Exception(f"Mapping for '{icon}' not known, available: {known_mappings.keys()}")
    mapping = known_mappings[icon]
    return  cg.new_Pvariable(config[CONF_RANDOM], mapping)

async def to_code_widget_text(idx, config):  
    text = config[CONF_VALUE]
    return  cg.new_Pvariable(config[CONF_RANDOM], text)

async def to_code_widget_time(idx, config):
    clock= await cg.get_variable(config[CONF_VALUE])
    format=config[CONF_FORMAT]
    return  cg.new_Pvariable(config[CONF_RANDOM], clock, format)

async def to_code_widget_where_value_is_entity(idx, config):
    entity= await cg.get_variable(config[CONF_VALUE])
    return  cg.new_Pvariable(config[CONF_RANDOM], entity)


async def  to_code_widget(idx, config):
    type = config[CONF_TYPE]
    if (type == CONF_ALERT_AGO):
        return await to_code_widget_where_value_is_plain(idx, config)
    elif (type == CONF_GROUP):
        return await to_code_widget_group(idx, config)
    elif (type == CONF_MDI):
        return await to_code_widget_mdi(idx, config)
    elif (type == CONF_TEXT):
        return await to_code_widget_text(idx, config)
    elif (type == CONF_TEXT_SENSOR):
        return await to_code_widget_where_value_is_entity(idx, config)
    elif (type == CONF_TIME):
        return await to_code_widget_time(idx, config)
    elif (type == CONF_SWITCH or type == CONF_BINARY_SENSOR):
        return await to_code_widget_where_value_is_entity(idx, config)
    else:
        raise Exception(f"Unknwon id: {id} config: {config}")

GLOBAL_MDI_KEY='c79818f3-e120-4fbd-b031-de64dc5bddc4'


async def to_code(config):
    known_mappings=CORE.data[GLOBAL_MDI_KEY]
    print(f"known_mappings: {known_mappings}")
    
    var = cg.new_Pvariable(config[CONF_ID])
    if  CONF_NAME_FONT in config:
        var_font_name = await cg.get_variable(config[CONF_NAME_FONT])   
        cg.add(var.set_font(var_font_name))
    for idx, widget in enumerate(config[CONF_WIDGETS]):
        widget_var= await to_code_widget(idx, widget)
        cg.add(var.add(widget_var))


ALERT_ON_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required('id'): cv.use_id(Group),
        cv.Optional('bmatrx_id', 'default_bmatrix'): cv.use_id(BMatrix),
    }
)
ALERT_OFF_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required('id'): cv.use_id(Group),
        cv.Optional('bmatrx_id', 'default_bmatrix'): cv.use_id(BMatrix),
    }
)
@automation.register_action("group.alert_on", AlertOnAction, ALERT_ON_ACTION_SCHEMA)
@automation.register_action("group.alert_off", AlertOffAction, ALERT_OFF_ACTION_SCHEMA)
async def alert_action_to_code(config, action_id, template_arg, args):
    print(f"config={config} action_id={action_id} template_arg={template_arg} args={args}")
    paren = await cg.get_variable(config[CONF_ID])
    matrix = await cg.get_variable(config['bmatrx_id'])
    return cg.new_Pvariable(action_id, template_arg, paren, matrix)

