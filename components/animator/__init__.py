import esphome.codegen as cg
from esphome.voluptuous_schema import _Schema
import esphome.config_validation as cv
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

clock24_ns = cg.esphome_ns.namespace("clock24")
Animator = clock24_ns.class_("Animator", cg.Component)



CONFIG_SCHEMA = _Schema(
    {
        cv.Required("director_id"): cv.use_id(Director24),
        cv.Required("baudrare"): cv.int_range(min=1200),
    }
)



async def to_code(config):
    pass