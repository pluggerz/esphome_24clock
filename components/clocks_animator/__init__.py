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

animator24_ns = cg.esphome_ns.namespace("animator24")
Animator = animator24_ns.class_("ClocksAnimator", cg.Component)

HandlesDistanceMode = animator24_ns.enum("HandlesDistanceEnum", is_class=True)
DISTANCE_CALCULATOR_MODES = {
    "RANDOM": HandlesDistanceMode.RANDOM,
    "SHORTEST": HandlesDistanceMode.SHORTEST,
    "LEFT": HandlesDistanceMode.LEFT,
    "RIGHT": HandlesDistanceMode.RIGHT,
}
validate_handles_distance_mode = cv.enum(DISTANCE_CALCULATOR_MODES, upper=True)


CONF_HANDLES_DISTANCE_MODE = 'handles_distance_mode'


CONFIG_SCHEMA = _Schema(
    {
        cv.Optional(CONF_ID, default='animator'): cv.declare_id(Animator),
        cv.Optional("director_id", default='director'): cv.use_id(Director24),
        cv.Optional(CONF_HANDLES_DISTANCE_MODE, 'RANDOM'): validate_handles_distance_mode

}
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_handles_distance_mode(DISTANCE_CALCULATOR_MODES[config[CONF_HANDLES_DISTANCE_MODE]]))
    
    director = await cg.get_variable(config['director_id'])
    cg.add(var.set_director(director))
    
    await cg.register_component(var, config)
    