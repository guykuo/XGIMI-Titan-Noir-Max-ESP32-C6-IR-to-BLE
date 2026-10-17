import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import display
from esphome.const import CONF_ID, CONF_LAMBDA

AUTO_LOAD = ["display"]
DEPENDENCIES = ["esp32"]

m5sticks3_display_ns = cg.esphome_ns.namespace("m5sticks3_display")
M5StickS3Display = m5sticks3_display_ns.class_(
    "M5StickS3Display",
    cg.PollingComponent,
    display.DisplayBuffer,
)

CONFIG_SCHEMA = display.FULL_DISPLAY_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(M5StickS3Display),
    }
).extend(cv.polling_component_schema("1s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    # register_display already handles component/display registration
    await display.register_display(var, config)

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA],
            [(display.DisplayRef, "it")],
            return_type=cg.void,
        )
        cg.add(var.set_writer(lambda_))
