const fs = require("fs");
const path = require("path");
const readline = require("readline");

function init() {
    const rl = readline.createInterface({
        input: process.stdin,
        output: process.stdout
    });

    rl.question("📌 Ingresa el nombre de tu bot: ", (botName) => {
        const botPath = path.join(__dirname, "../../bots", botName);

        if (fs.existsSync(botPath)) {
            console.log("❌ Ese bot ya existe.");
            rl.close();
            return;
        }

        // Crear la carpeta del bot y las subcarpetas necesarias
        fs.mkdirSync(botPath, { recursive: true });
        fs.mkdirSync(path.join(botPath, "commands"), { recursive: true });
        fs.mkdirSync(path.join(botPath, "functions"), { recursive: true });

        // Crear archivo default reply.js en functions, si no existe
        const replyFilePath = path.join(botPath, "functions", "reply.js");
        if (!fs.existsSync(replyFilePath)) {
            const replyContent = `module.exports = {
    execute: (message, reply) => {
        const user = message.author.username;
        const response = reply.replace("{user}", user);
        message.reply(response);
    }
};`;
            fs.writeFileSync(replyFilePath, replyContent);
        }

        rl.question("🔑 Ingresa el token de tu bot: ", (token) => {
            const encodedToken = Buffer.from(token).toString("base64");
            fs.writeFileSync(path.join(botPath, "token.bot.cxl"), encodedToken);

            // Crear archivo de configuración cordlang.conf.jsonc
            const config = {
                name: botName,
                version: "1.0.0",
                description: "Un bot hecho con CordLang",
                prefix: "!",
                slash_commands: true,
                main_file: "main.cxl",
                commands_folder: "commands",
                token_file: "token.bot.cxl",
                module: true,
                debug: false,
                log_channel: null
            };
            fs.writeFileSync(
                path.join(botPath, "cordlang.conf.jsonc"),
                JSON.stringify(config, null, 2)
            );

            // Crear el script principal main.cxl con el nuevo formato modular
            const mainContent = `imports {
  token
  prefix
}

commands [
  command {
    name: "hello"
    reply: "hello {user}"
    slash_command: true
    options_slash {
      name: "hello"
      description: "Saludo al usuario"
    }
  }
]

bot {
  ready("Bot de prueba está en línea!", "Error al iniciar el bot")
}
`;
            fs.writeFileSync(path.join(botPath, "main.cxl"), mainContent);

            console.log(`✅ Bot "${botName}" creado en /bots/${botName}/`);
            console.log("🔐 El token se ha almacenado en Base64 y la configuración en cordlang.conf.jsonc.");
            rl.close();
        });
    });
}

module.exports = { init };
