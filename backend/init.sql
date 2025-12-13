-- Initial database setup for Sidechain
-- This file is automatically run when the PostgreSQL container starts for the first time

-- Create extensions if needed
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- Create the gorse database for recommendation engine
CREATE DATABASE gorse OWNER sidechain;

-- The database and user are already created by the POSTGRES_USER/POSTGRES_DB env vars
-- This file can be used for additional setup like creating schemas, initial data, etc.

-- Create a simple health check
SELECT 'Database initialized successfully' AS status;

-- Now seed the gorse database with initial data to prevent empty dataset panic
\c gorse

-- Create Gorse tables (matching Gorse's expected schema)
CREATE TABLE IF NOT EXISTS users (
    user_id VARCHAR(255) PRIMARY KEY,
    labels JSONB DEFAULT '[]',
    subscribe JSONB DEFAULT '[]',
    comment TEXT DEFAULT ''
);

CREATE TABLE IF NOT EXISTS items (
    item_id VARCHAR(255) PRIMARY KEY,
    is_hidden BOOLEAN DEFAULT FALSE,
    categories JSONB DEFAULT '[]',
    time_stamp TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    labels JSONB DEFAULT '[]',
    comment TEXT DEFAULT ''
);

CREATE TABLE IF NOT EXISTS feedback (
    feedback_type VARCHAR(255) NOT NULL,
    user_id VARCHAR(255) NOT NULL,
    item_id VARCHAR(255) NOT NULL,
    time_stamp TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    comment TEXT DEFAULT '',
    PRIMARY KEY (feedback_type, user_id, item_id)
);

-- Insert seed data to prevent empty dataset panic
INSERT INTO users (user_id, labels) VALUES
    ('seed_user_1', '["producer", "electronic"]'),
    ('seed_user_2', '["producer", "hiphop"]'),
    ('seed_user_3', '["producer", "ambient"]')
ON CONFLICT DO NOTHING;

INSERT INTO items (item_id, labels, categories) VALUES
    ('seed_item_1', '["electronic", "120bpm"]', '["loop"]'),
    ('seed_item_2', '["hiphop", "90bpm"]', '["loop"]'),
    ('seed_item_3', '["ambient", "80bpm"]', '["loop"]')
ON CONFLICT DO NOTHING;

INSERT INTO feedback (feedback_type, user_id, item_id) VALUES
    ('like', 'seed_user_1', 'seed_item_2'),
    ('view', 'seed_user_2', 'seed_item_1'),
    ('like', 'seed_user_3', 'seed_item_1')
ON CONFLICT DO NOTHING;

SELECT 'Gorse database seeded successfully' AS status;
