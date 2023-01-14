import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import (
    CONF_NUM_LEDS,
    CONF_OUTPUT_ID,
)


clocks_ligh_ns = cg.esphome_ns.namespace("clocks_light")
ClocksLightOutput = clocks_ligh_ns.class_("ClocksLightOutput", light.AddressableLight)

def _validate(config):
    return config

CONFIG_SCHEMA = cv.All(
    light.ADDRESSABLE_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(ClocksLightOutput),
            cv.Required(CONF_NUM_LEDS): cv.positive_not_null_int,
        }
    ).extend(cv.COMPONENT_SCHEMA),
)


async def to_code(config):
    pass
    out_type = ClocksLightOutput
    rhs = out_type.new()
    var = cg.Pvariable(config[CONF_OUTPUT_ID], rhs, out_type)
    await light.register_light(var, config)
    await cg.register_component(var, config)

    cg.add(var.add_leds(config[CONF_NUM_LEDS]))
    
