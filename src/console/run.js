const fs = require("fs");
const path = require("path");
const logger = require("../utils/logger");
const botModule = require("../bot");

function run(args) {
  const botName = args[0];
  if (!botName) {
    logger.error("❌ Debes especificar el nombre del bot. Ejemplo: cord run miBot");
    return;
  }

  const botPath = path.join("bots", botName);

  if (!fs.existsSync(botPath)) {
    logger.error(`❌ El bot "${botName}" no existe. Asegúrate de haberlo creado con 'cord init'.`);
    return;
  }

  botModule.startBot(botPath);
}

module.exports = { run };
