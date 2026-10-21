import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

st77922_touch_ns = cg.esphome_ns.namespace("st77922_touch")
ST77922TouchBridge = st77922_touch_ns.class_("ST77922TouchBridge", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ST77922TouchBridge),
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
