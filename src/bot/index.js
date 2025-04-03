const fs = require("fs");
const path = require("path");
const { Client, GatewayIntentBits } = require("discord.js");
const { parseCordLang } = require("../parser");

function startBot(botPath) {
    const configPath = path.join(botPath, "cordlang.conf.jsonc");
    if (!fs.existsSync(configPath)) {
        console.log("❌ No se encontró cordlang.conf.jsonc.");
        return;
    }
    
    const config = JSON.parse(fs.readFileSync(configPath, "utf-8"));
    const mainPath = path.join(botPath, config.main_file);
    if (!fs.existsSync(mainPath)) {
        console.log("❌ No se encontró el archivo principal (main.cxl).");
        return;
    }
    
    const parsedData = parseCordLang(mainPath);
    
    // Leer y decodificar el token
    const tokenPath = path.join(botPath, config.token_file);
    if (!fs.existsSync(tokenPath)) {
        console.log("❌ No se encontró el token del bot.");
        return;
    }
    const encodedToken = fs.readFileSync(tokenPath, "utf-8").trim();
    const token = Buffer.from(encodedToken, "base64").toString("utf-8");
    
    const client = new Client({ intents: [GatewayIntentBits.Guilds, GatewayIntentBits.GuildMessages, GatewayIntentBits.MessageContent] });
    
    client.once("ready", () => {
        console.log(parsedData.readyMessages.success);
    });
    
    // Cargar función modular para respuestas
    let replyFunction;
    try {
        replyFunction = require(path.join(botPath, "functions", "reply"));
    } catch (err) {
        console.log("❌ No se pudo cargar la función de reply:", err);
        return;
    }
    
    client.on("messageCreate", async (message) => {
        if (message.author.bot) return;
        parsedData.commands.forEach((cmd) => {
            if (message.content === config.prefix + cmd.name) {
                replyFunction.execute(message, cmd.reply);
            }
        });
    });
    
    client.login(token).catch(() => {
        console.log(parsedData.readyMessages.error);
    });
}

module.exports = { startBot };
