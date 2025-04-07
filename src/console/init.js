const fs = require("fs");
const path = require("path");
const readline = require("readline");
const logger = require("../utils/logger");

function init() {
  const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout
  });

  logger.log("Bienvenido a CordLang 🚀");

  rl.question("📌 Ingresa el nombre de tu bot: ", (name) => {
    rl.question("🔑 Ingresa el token de tu bot: ", (token) => {
      rl.question("🔣 Ingresa el prefijo de tu bot: ", (prefix) => {
        rl.question("📜 Ingresa la descripción de tu bot: ", (description) => {
          const botDir = path.join("bots", name);

          if (fs.existsSync(botDir)) {
            logger.warn(`El bot '${name}' ya existe.`);
            rl.close();
            return;
          }

          fs.mkdirSync(botDir, { recursive: true });
          fs.mkdirSync(path.join(botDir, "commands"), { recursive: true });

          const config = {
            name: name,
            version: "1.0.0",
            description: description || "Un bot hecho con CordLang",
            prefix: prefix,
            slash_commands: true,
            main_file: "main.cxl",
            commands_folder: "commands",
            token_file: "token.bot.cxl",
            module: true,
            debug: false,
            log_channel: null
          };

          fs.writeFileSync(path.join(botDir, "cordlang.conf.jsonc"), JSON.stringify(config, null, 4));
          fs.writeFileSync(path.join(botDir, "token.bot.cxl"), Buffer.from(token).toString("base64"));

          logger.success(`✅ Bot "${name}" creado en /bots/${name}/`);
          logger.success("🔐 El token se ha almacenado en Base64 y la configuración en cordlang.conf.jsonc.");
          rl.close();
        });
      });
    });
  });
}

module.exports = { init };
