const fs = require("fs");

function parseCordLang(filePath) {
    if (!fs.existsSync(filePath)) {
        throw new Error("❌ No se encontró el archivo main.cxl");
    }

    const content = fs.readFileSync(filePath, "utf-8");

    // Extraer la sección imports (lista de variables a importar)
    const importsMatch = content.match(/imports\s*{\s*([\s\S]*?)\s*}/);
    const imports = importsMatch
        ? importsMatch[1].trim().split("\n").map(line => line.trim()).filter(line => line)
        : [];

    // Extraer la sección commands (lista de comandos)
    const commandsMatch = content.match(/commands\s*\[\s*([\s\S]*?)\s*\]/);
    const commands = [];
    if (commandsMatch) {
        // Expresión regular para capturar cada comando
        const commandRegex = /command\s*{\s*name\s*:\s*"([^"]+)"\s*reply\s*:\s*"([^"]+)"\s*slash_command\s*:\s*(true|false)\s*options_slash\s*{\s*name\s*:\s*"([^"]+)"\s*description\s*:\s*"([^"]+)"\s*}\s*}/g;
        let match;
        while ((match = commandRegex.exec(commandsMatch[1])) !== null) {
            commands.push({
                name: match[1],
                reply: match[2],
                slashCommand: match[3] === "true",
                slashOptions: {
                    name: match[4],
                    description: match[5]
                }
            });
        }
    }

    // Extraer la sección bot (mensajes de ready)
    const botMatch = content.match(/bot\s*{\s*ready\s*\(\s*"([^"]+)"\s*,\s*"([^"]+)"\s*\)/);
    const readyMessages = botMatch ? { success: botMatch[1], error: botMatch[2] } : { success: "Bot en línea!", error: "Error al iniciar el bot" };

    return { imports, commands, readyMessages };
}

module.exports = { parseCordLang };
