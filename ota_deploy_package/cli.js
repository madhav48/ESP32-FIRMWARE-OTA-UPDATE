#!/usr/bin/env node
const { deployPipeline } = require('./scripts/deploy');
const os = require('os');
const logger = require('./services/logger');


// Parse CLI arguments
const args = process.argv.slice(2);

// Extract options
const getArgValue = (key) => {
  const prefix = `--${key}=`;
  const arg = args.find(arg => arg.startsWith(prefix));
  return arg ? arg.replace(prefix, '') : undefined;
};

const firmwareVersion = getArgValue('version');
const changelog = getArgValue('changelog');
const deployedBy = os.userInfo().username;

(async () => {
  if (!changelog) {
    logger.error('\n Missing required arguments.\n');
    logger.info('Usage: deploy --version=<version> --changelog="<description>"\n');
    process.exit(1);
  }

  await deployPipeline({
    firmwareVersion,
    changelog,
    deployedBy,
  });
})();
