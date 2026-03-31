import 'dotenv/config';
import { drizzle } from 'drizzle-orm/libsql';
import { createClient } from '@libsql/client';
import * as schema from './schema';
import { env } from '~~/server/env';

const client = createClient({ url: env.DB_FILE_NAME });

export const db = drizzle(client, { schema });
