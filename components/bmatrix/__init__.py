# depending on https://materialdesignicons.com/
from cgitb import text
from logging import Logger
import logging
from esphome.voluptuous_schema import _Schema
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display, sensor, binary_sensor, switch, image, font, time, text_sensor
from esphome.const import (
    CONF_BINARY_SENSOR,
    CONF_DEVICE_CLASS,
    CONF_DISABLED_BY_DEFAULT,
    CONF_FILE,
    CONF_FORMAT,
    CONF_GLYPHS,
    CONF_ID,
    CONF_NAME,
    CONF_GROUP,
    CONF_NAME_FONT,
    CONF_RANDOM,
    CONF_SENSOR,
    CONF_SIZE,
    CONF_SOURCE,
    CONF_TYPE,
    CONF_VISIBLE,
    DEVICE_CLASS_MOTION,
    DEVICE_CLASS_TEMPERATURE,
)

_LOGGER = logging.getLogger(__name__)


DEPENDENCIES = ["display"]
CODEOWNERS = ["@hvandenesker"]

CONF_DISPLAY = "display"
CONF_MDI = "mdi"


bmatrix_ns = cg.esphome_ns.namespace("bmatrix")
BMatrix = bmatrix_ns.class_("BMatrix", cg.Component)
Group = bmatrix_ns.class_("Group")
Mdi = bmatrix_ns.class_("Mdi")

CONFIG_SCHEMA = _Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(BMatrix),
        cv.Required(CONF_DISPLAY): cv.use_id(display.DisplayBuffer),
        cv.Required(CONF_NAME_FONT): cv.use_id(font.Font),
        cv.Required(CONF_GROUP): cv.use_id(Group),
        cv.Optional(CONF_MDI, 'default_mdi'): cv.use_id(Mdi)
    }
)

async def to_code(config):
    
    var_display = await cg.get_variable(config[CONF_DISPLAY])
    var = cg.new_Pvariable(config[CONF_ID], var_display)
    var_group = await cg.get_variable(config[CONF_GROUP])
    var_font_name = await cg.get_variable(config[CONF_NAME_FONT])
    var_mdi = await cg.get_variable(config[CONF_MDI])
    
    cg.add(var.set_mdi(var_mdi))
    cg.add(var.set_font(var_font_name))
    cg.add(var.set_canvas(var_group))


