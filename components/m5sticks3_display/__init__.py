from esphome.core import CORE, Library

# Wrap the library string descriptors inside explicit Library class instances
CORE.add_library(Library("m5stack/M5Unified", "^0.2.3"))
CORE.add_library(Library("m5stack/M5GFX", "^0.2.3"))

CODEOWNERS = ["@openai"]
