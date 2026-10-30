BEGIN;

-- Drop child tables before parent tables
DROP TABLE IF EXISTS tiny_bin.posts;
DROP TABLE IF EXISTS tiny_bin.users;

-- Remove the application schema
DROP SCHEMA IF EXISTS tiny_bin;

COMMIT;
