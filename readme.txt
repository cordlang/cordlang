cordlang/                
├── src/                
│   ├── index.js         // Entrada CLI
│   ├── console/         // Módulos de CLI (init, run, update)
│   │   ├── index.js     
│   │   ├── init.js      
│   │   ├── run.js       
│   │   └── update.js    
│   ├── bot/             // Módulo que maneja el bot
│   │   ├── index.js     // Lógica del bot que usa módulos internos
│   │   └── functions/   // Funciones internas (reply.js, etc.)
│   │         └── reply.js
│   ├── utils/
│       ├── logguer.js   // Funciones para darle color y estilo a la consola
│   └── parser/          // Módulo para interpretar archivos .cxl
│         └── index.js     
├── bots/                // Carpeta que contendrá los bots creados (cada uno en su subcarpeta)
│   └── <nombre_bot>/   
│         ├── cordlang.conf.jsonc  
│         ├── main.cxl              
│         ├── token.bot.cxl         
│         └── commands/             // Carpeta vacía para comandos personalizados
├── package.json         
└── node_modules/        
