const { Client } = require('pg');
const path = require('path');
require('dotenv').config({ path: path.resolve(__dirname, '../.env') });
const logger = require('../services/logger');
const fs = require('fs');


const certPath = path.resolve(__dirname, '../certs/rds-global-bundle.pem');



// Save firmware metadata to PostgreSQL
async function saveMetadata({ version, firmware_url, signature_url, changelog, deployed_by, checksum }) {
  const client = new Client({
    connectionString: process.env.POSTGRES_URL,
    ssl: {
      ca: fs.readFileSync(certPath).toString(),
      rejectUnauthorized: true
    }
  });

  const relation = process.env.FIRMWARE_RELATION;
  await client.connect();

  const query = `
    INSERT INTO ${relation} 
    (firmware_version, firmware_path, signature_path, changelog, deployed_by, checksum, created_at)
    VALUES ($1, $2, $3, $4, $5, $6, NOW())
  `;

  await client.query(query, [version, firmware_url, signature_url, changelog, deployed_by, checksum]);

  await client.end();
  logger.success('Metadata stored in PostgreSQL.');
}


// Check if a version already exists in the database
async function checkVersionExists(version) {
  const client = new Client({
    connectionString: process.env.POSTGRES_URL,
    ssl: {
      ca: fs.readFileSync(certPath).toString(),
      rejectUnauthorized: true
    }
  });

  const relation = process.env.FIRMWARE_RELATION;
  await client.connect();

  const query = `SELECT 1 FROM ${relation} WHERE firmware_version = $1`;
  const res = await client.query(query, [version]);

  await client.end();
  return res.rowCount > 0;
}


// Fetch the latest firmware version and increment it
async function getLatestVersion() {
  const client = new Client({
    connectionString: process.env.POSTGRES_URL,
    ssl: {
      ca: fs.readFileSync(certPath).toString(),
      rejectUnauthorized: true
    }
  });

  const relation = process.env.FIRMWARE_RELATION;
  await client.connect();

  const query = `SELECT firmware_version FROM ${relation} ORDER BY created_at DESC LIMIT 1`;
  const res = await client.query(query);
  await client.end();

  if (res.rowCount === 0) return '1.0.0';

  const latest = res.rows[0].firmware_version;
  const parts = latest.split('.').map(Number);

  if (parts.length !== 3 || parts.some(isNaN)) return '1.0.0';

  parts[2]++; // increment patch version
  return parts.join('.');
}

module.exports = { saveMetadata, checkVersionExists, getLatestVersion };
