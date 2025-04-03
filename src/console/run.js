const fs = require("fs");
const path = require("path");
const { startBot } = require("../bot");

function run(args) {
    const botName = args[0];
    if (!botName) {
        console.log("❌ Debes especificar el nombre del bot. Ejemplo: cord run miBot");
        return;
    }

    const botPath = path.join(__dirname, "../../bots", botName);

    if (!fs.existsSync(botPath)) {
        console.log(`❌ El bot "${botName}" no existe. Asegúrate de haberlo creado con 'cord init'.`);
        return;
    }

    startBot(botPath);
}

module.exports = { run };
