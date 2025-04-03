#!/usr/bin/env node
const args = process.argv.slice(2);
const command = args[0];
const consoleCommands = require("./console");

if (!command) {
    console.log("❌ Debes especificar un comando.");
    process.exit(1);
}

if (consoleCommands[command]) {
    consoleCommands[command](args.slice(1));
} else {
    console.log(`❌ Comando "${command}" no reconocido.`);
    console.log("📌 Comandos disponibles:");
    console.log("   cord init       → Crea un nuevo bot");
    console.log("   cord run <bot>  → Ejecuta un bot");
    console.log("   cord update     → Actualiza un bot");
    process.exit(1);
}
