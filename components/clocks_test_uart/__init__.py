import esphome.codegen as cg
from esphome.components import output
from esphome.voluptuous_schema import _Schema
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
 )

CONF_COUNT_START = "count_start"
CONF_PERFORMERS = "performers"
CONF_HANDLE0 = "H0"
CONF_HANDLE1 = "H1"
CONF_ANIMATION_SLAVE_ID = "animation_slave_id"
CONF_BAUDRATE='baud_rate'


DEPENDENCIES = ["pcf8574", "clocks_shared"]
AUTO_LOAD = [ ]

CODEOWNERS = ["@hvandenesker"]

clock24_ns = cg.esphome_ns.namespace("clock24")
Director24 = clock24_ns.class_("DirectorTestUart", cg.Component)

CONFIG_SCHEMA = _Schema(
    {
        cv.Optional(CONF_ID, default='director'): cv.declare_id(Director24),
        cv.Optional(CONF_BAUDRATE, 9600): cv.int_range(min=1200),
    }
)

async def to_code_light(conf):
    var = cg.new_Pvariable(conf[CONF_ID])
    await output.register_output(var, conf)

async def to_code(config):
    director = cg.new_Pvariable(config[CONF_ID])
        
    cg.add(director.set_baudrate(config[CONF_BAUDRATE]))

    await cg.register_component(director, config)

