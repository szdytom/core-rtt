import process from 'node:process';
import * as dotenv from 'dotenv';
import { z } from 'zod';

dotenv.config({ path: '../.env', quiet: true });

const envSchema = z.object({
  DB_FILE_NAME: z.url(),

  S3_SERVER_URL: z.string(),
  S3_ACCESS_KEY_ID: z.string(),
  S3_SECRET_ACCESS_KEY: z.string(),
  S3_BUCKET_NAME: z.string(),
  S3_REGION: z.string(),
  S3_PRESIGNED_URL_EXPIRATION_SECONDS: z.int().default(3600), // Expires in 1 hour

  GITHUB_CLIENT_ID: z.string(),
  GITHUB_CLIENT_SECRET: z.string(),
});

const envParse = envSchema.safeParse(process.env);

if (!envParse.success) {
  console.error('[ERROR] Invalid environment variables:', z.treeifyError(envParse.error));
  process.exit(1);
}

export const env = envParse.data;
