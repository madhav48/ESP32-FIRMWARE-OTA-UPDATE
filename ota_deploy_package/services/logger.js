const chalk = require('chalk');

module.exports = {
  info: (msg) => console.log(chalk.yellow(msg)),
  success: (msg) => console.log(chalk.green(msg)),
  error: (msg) => console.error(chalk.red(msg))
};
