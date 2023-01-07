import esphome.codegen as cg
from esphome.const import CONF_LIGHT,CONF_RED,CONF_GREEN,CONF_BLUE
from esphome.components import output, light
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
Director24 = clock24_ns.class_("Director", cg.Component)



def cv_performer_id_check(id):
    id =int(id)
    if id < 0 or id > 23: 
        raise Exception(f"Id of performer should be one of: 0,1,..,23! Not: {id}")
    return id

def cv_slave_conf_check(id, defPerformerConf):
    d = dict()
    if CONF_HANDLE0 in defPerformerConf:
        d[CONF_HANDLE0] = defPerformerConf[CONF_HANDLE0]
    else:
        d[CONF_HANDLE0] = 0

    if CONF_HANDLE1 in defPerformerConf:
        d[CONF_HANDLE1] = defPerformerConf[CONF_HANDLE1]
    else:
        d[CONF_HANDLE1] = 0

    id=int(id)
    d[CONF_ANIMATION_SLAVE_ID] = id
    if id < 0 or id > 23: 
        raise Exception(f"Id of performer should be one of: 0,1,..,23! Not: {id}")
    return d

def cv_performers_check(conf):
    d = dict()
    defPerformersConf = conf["*"]
    print("defPerformersConf", defPerformersConf)
    for id, slaveConf in conf.items():
        if (id != "*"):
            d[cv_performer_id_check(id)] = cv_slave_conf_check(
                slaveConf, defPerformersConf)
    return d

CONFIG_SCHEMA = _Schema(
    {
        cv.Optional(CONF_ID, default='director'): cv.declare_id(Director24),
#        cv.GenerateID(): cv.declare_id(OClockStubController),
#        cv.Optional(CONF_COUNT_START, -1): cv.int_range(min=-1, max=64),
        cv.Optional(CONF_BAUDRATE, 9600): cv.int_range(min=1200),
#        cv.Optional('turn_speed', 4): cv.int_range(min=0, max=8),
#        cv.Optional('turn_steps', 10): cv.int_range(min=0, max=90),
        cv.Required(CONF_PERFORMERS): cv_performers_check,
#        cv.Required('components'): COMPONENTS_SCHEMA,
        # cv.Optional(CONF_BRIGHTNESS, default={}): BRIGHTNESS_SCHEMA,
#        cv.Required(CONF_LIGHT): RGBLIGHT_SCHEMA,
    }
)

def to_code_slave(master, physicalSlaveId, slaveConf):
    print(f"S{physicalSlaveId} maps to: ${slaveConf}")
    animatorId=slaveConf[CONF_ANIMATION_SLAVE_ID];
    if animatorId < 0 or animatorId > 23: 
        raise Exception("Id of performer should be one of: 0,1,..,23")
    cg.add(master.performer(physicalSlaveId).set_animator_id(animatorId))
    cg.add(master.performer(physicalSlaveId).set_magnet_offsets(slaveConf[CONF_HANDLE0],slaveConf[CONF_HANDLE1]))


#LighFloatOutput = oclock_ns.class_("LighFloatOutput", output.FloatOutput)

#LIGHT_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
#    {
#        cv.GenerateID(): cv.declare_id(LighFloatOutput)
#    }).extend(cv.COMPONENT_SCHEMA)

#RGBLIGHT_SCHEMA = cv.All({
#    cv.Required(CONF_RED): LIGHT_SCHEMA,
#    cv.Required(CONF_GREEN): LIGHT_SCHEMA,
#    cv.Required(CONF_BLUE): LIGHT_SCHEMA,
#})

async def to_code_light(conf):
    var = cg.new_Pvariable(conf[CONF_ID])
    await output.register_output(var, conf)

async def to_code(config):
#    ligthConf = config[CONF_LIGHT]
#    await to_code_light(ligthConf[CONF_RED])
#    await to_code_light(ligthConf[CONF_GREEN])
#    await to_code_light(ligthConf[CONF_BLUE])

    # master
    master = cg.new_Pvariable(config[CONF_ID])
    
    for slaveId, slaveConf in config[CONF_PERFORMERS].items():
        to_code_slave(master, slaveId, slaveConf)

    cg.add(master.set_baudrate(config[CONF_BAUDRATE]))

    await cg.register_component(master, config)

