const fs = require("fs");
const path = require("path");
const { Client, GatewayIntentBits } = require("discord.js");
const parser = require("../parser");
const logger = require("../utils/logger");
// Importar la función de respuesta interna
const replyFunction = require("./functions/reply");

function startBot(botPath) {
  const configPath = path.join(botPath, "cordlang.conf.jsonc");
  if (!fs.existsSync(configPath)) {
    logger.error("❌ No se encontró cordlang.conf.jsonc.");
    return;
  }

  const config = JSON.parse(fs.readFileSync(configPath, "utf-8"));
  const mainPath = path.join(botPath, config.main_file);
  if (!fs.existsSync(mainPath)) {
    logger.error("❌ No se encontró el archivo principal (main.cxl).");
    return;
  }

  const parsedData = parser.parseCordLang(mainPath);

  // Leer y decodificar el token
  const tokenPath = path.join(botPath, config.token_file);
  if (!fs.existsSync(tokenPath)) {
    logger.error("❌ No se encontró el token del bot.");
    return;
  }
  const encodedToken = fs.readFileSync(tokenPath, "utf-8").trim();
  const token = Buffer.from(encodedToken, "base64").toString("utf-8");

  const client = new Client({
    intents: [
      GatewayIntentBits.Guilds,
      GatewayIntentBits.GuildMessages,
      GatewayIntentBits.MessageContent
    ]
  });

  client.once("ready", () => {
    logger.success(parsedData.readyMessages.success);
  });

  client.on("messageCreate", async (message) => {
    if (message.author.bot) return;
    parsedData.commands.forEach((cmd) => {
      if (message.content === config.prefix + cmd.name) {
        replyFunction.execute(message, cmd.reply);
      }
    });
  });

  client.login(token).catch(() => {
    logger.error(parsedData.readyMessages.error);
  });
}

module.exports = { startBot };
