-- Initial database setup for Sidechain
-- This file is automatically run when the PostgreSQL container starts for the first time

-- Create extensions if needed
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- The database and user are already created by the POSTGRES_USER/POSTGRES_DB env vars
-- This file can be used for additional setup like creating schemas, initial data, etc.

-- Create a simple health check
SELECT 'Database initialized successfully' AS status;
