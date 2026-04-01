import 'dotenv/config';
import { drizzle } from 'drizzle-orm/libsql';
import * as schema from './schema';
import { env } from '~~/server/env';

export const db = drizzle({
  connection: {
    url: env.DB_FILE_NAME,
  },
  schema,
});
