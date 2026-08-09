import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32_ble, esp32_ble_server
from esphome.const import CONF_ID

CODEOWNERS = []
DEPENDENCIES = ["esp32_ble_server"]

CONF_BLE_SERVER_ID = "ble_server_id"
CONF_KEYBOARD_REPORT_ID = "keyboard_report_id"
CONF_CONSUMER_REPORT_ID = "consumer_report_id"
CONF_REMOTE_NAME = "remote_name"
CONF_TAP_DURATION_MS = "tap_duration_ms"
CONF_WAKE_TOKEN = "wake_token"

xgimi_remote_ns = cg.esphome_ns.namespace("xgimi_remote")
XgimiRemote = xgimi_remote_ns.class_("XgimiRemote", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(XgimiRemote),
        cv.GenerateID(esp32_ble.CONF_BLE_ID): cv.use_id(esp32_ble.ESP32BLE),
        cv.Required(CONF_BLE_SERVER_ID): cv.use_id(esp32_ble_server.BLEServer),
        cv.Required(CONF_KEYBOARD_REPORT_ID): cv.use_id(
            esp32_ble_server.BLECharacteristic
        ),
        cv.Required(CONF_CONSUMER_REPORT_ID): cv.use_id(
            esp32_ble_server.BLECharacteristic
        ),
        cv.Optional(CONF_REMOTE_NAME, default="M5Stack Atom Lite"): cv.All(
            cv.string_strict, cv.Length(min=1, max=20)
        ),
        cv.Optional(CONF_TAP_DURATION_MS, default=100): cv.int_range(min=20, max=1000),
        cv.Required(CONF_WAKE_TOKEN): cv.All(
            cv.ensure_list(cv.hex_uint8_t), cv.Length(min=15, max=15)
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    ble = await cg.get_variable(config[esp32_ble.CONF_BLE_ID])
    server = await cg.get_variable(config[CONF_BLE_SERVER_ID])
    keyboard_report = await cg.get_variable(config[CONF_KEYBOARD_REPORT_ID])
    consumer_report = await cg.get_variable(config[CONF_CONSUMER_REPORT_ID])

    cg.add(var.set_ble(ble))
    cg.add(var.set_server(server))
    cg.add(var.set_keyboard_report(keyboard_report))
    cg.add(var.set_consumer_report(consumer_report))
    cg.add(var.set_remote_name(config[CONF_REMOTE_NAME]))
    cg.add(var.set_tap_duration_ms(config[CONF_TAP_DURATION_MS]))
    cg.add(var.set_wake_token(config[CONF_WAKE_TOKEN]))

    esp32_ble.register_gap_event_handler(ble, var)
    esp32_ble.register_gatts_event_handler(ble, var)
    esp32_ble.register_bt_logger(esp32_ble.BTLoggers.GATT, esp32_ble.BTLoggers.SMP)
