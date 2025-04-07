cordlang/
│
├── src/
│   ├── index.ts         // Punto de entrada principal
│   ├── parser.ts        // Parsea el lenguaje .cxl y produce instrucciones
│   ├── runtime.ts       // Ejecuta las instrucciones con discord.js
│   └── functions/       // Funciones personalizadas para Discord
│         ├── reply.ts           // Función para enviar respuestas
│         ├── noMention.ts       // Función para verificar que no haya menciones
│         └── autorID.ts         // Función para obtener el ID del autor del mensaje
│         └── addButton.ts       // Función para añadir un boton
│         └── addCmdReactions.ts // Función para reaccion
│         └── addEmoji.ts        // Función para añadir emojis
│         └── addField.ts        // Función para field
│
├── example/
│   └── bot.cxl          // Script de ejemplo en CordLang
│
├── package.json
├── tsconfig.json
└── README.md
