CREATE TABLE firmware_deployments (
    id SERIAL PRIMARY KEY,
    firmware_version VARCHAR(50) NOT NULL,
    firmware_path TEXT NOT NULL,
    signature_path TEXT NOT NULL,
    changelog TEXT,
    deployed_by VARCHAR(100),
    checksum VARCHAR(128),
    created_at TIMESTAMP DEFAULT NOW()
);
