const chalk = require("chalk").default;

const logger = {
    log: (message) => {
        console.log(`${chalk.blue("cordlang:log")} ${chalk.green("[log]")} ${message}`);
    },
    error: (message) => {
        console.error(`${chalk.red("cordlang:error")} ${chalk.red("[error]")} ${message}`);
    },
    warn: (message) => {
        console.warn(`${chalk.yellow("cordlang:warn")} ${chalk.yellow("[warn]")} ${message}`);
    },
    success: (message) => {
        console.log(`${chalk.green("cordlang:success")} ${chalk.green("[success]")} ${message}`);
    }
};

module.exports = logger;
