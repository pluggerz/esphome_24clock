import esphome.codegen as cg
from esphome.voluptuous_schema import _Schema
from esphome import automation
import esphome.config_validation as cv
from esphome.core import CORE, coroutine_with_priority
from esphome.const import (
    CONF_ID,
 )
#import esphome.components.clock24.Director24 as Director24

# our dependency
clock24_ns = cg.esphome_ns.namespace("clock24")
Director24 = clock24_ns.class_("Director", cg.Component)

DEPENDENCIES = ["clocks_director"]

AUTO_LOAD = [ ]

CODEOWNERS = ["@hvandenesker"]

lighting_ns = cg.esphome_ns.namespace("lighting")
LightingController = lighting_ns.class_("LightingController", cg.Component)



CONFIG_SCHEMA = _Schema(
    {
        cv.Optional(CONF_ID, default='lighting_controller'): cv.declare_id(LightingController),
        cv.Optional("director_id", default='director'): cv.use_id(Director24),
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    
    director = await cg.get_variable(config['director_id'])
    cg.add(var.set_director(director))
    
    await cg.register_component(var, config)
    