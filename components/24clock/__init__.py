import esphome.codegen as cg
from esphome.voluptuous_schema import _Schema
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
 )

# time
from esphome.components import time as time_

DEPENDENCIES = ["pcf8574", "time"]

AUTO_LOAD = [ ]

CODEOWNERS = ["@hvandenesker"]

clock24_ns = cg.esphome_ns.namespace("clock24")

Director24 = clock24_ns.class_("Director", cg.Component)

CONF_TIME_ID = 'time_id'

CONFIG_SCHEMA = _Schema(
    {
        cv.Optional(CONF_ID, default='director'): cv.declare_id(Director24),
        cv.Optional(CONF_TIME_ID, default='current_time'): cv.use_id(time_.RealTimeClock),
#        cv.GenerateID(): cv.declare_id(OClockStubController),
#        cv.Optional(CONF_COUNT_START, -1): cv.int_range(min=-1, max=64),
#        cv.Optional('baud_rate', 1200): cv.int_range(min=1200),
#        cv.Optional('turn_speed', 4): cv.int_range(min=0, max=8),
#        cv.Optional('turn_steps', 10): cv.int_range(min=0, max=90),
#        cv.Required(CONF_SLAVES): cv_slaves_check,
#        cv.Required('components'): COMPONENTS_SCHEMA,
        # cv.Optional(CONF_BRIGHTNESS, default={}): BRIGHTNESS_SCHEMA,
#        cv.Required(CONF_LIGHT): RGBLIGHT_SCHEMA,
    }
)


async def to_code(config):
    # master
    master = cg.new_Pvariable(config[CONF_ID])
    
    var = await cg.get_variable(config[CONF_TIME_ID])
    print(f"Enitity for time: {var}")
    cg.add(master.set_time(var))

    await cg.register_component(master, config)

