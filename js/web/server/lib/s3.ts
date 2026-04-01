import { GetObjectCommand, PutObjectCommand, S3 } from '@aws-sdk/client-s3';
import { getSignedUrl } from '@aws-sdk/s3-request-presigner';
import { env } from '~~/server/env';

const s3 = new S3({
  endpoint: env.S3_SERVER_URL,
  region: env.S3_REGION,
  credentials: {
    accessKeyId: env.S3_ACCESS_KEY_ID,
    secretAccessKey: env.S3_SECRET_ACCESS_KEY,
  },
});

export async function getUploadUrl(key: string) {
  return await getSignedUrl(s3, new PutObjectCommand({
    Bucket: env.S3_BUCKET_NAME,
    Key: key,
  }), {
    expiresIn: env.S3_PRESIGNED_URL_EXPIRATION_SECONDS,
  });
}

export async function getDownloadUrl(key: string) {
  return await getSignedUrl(s3, new GetObjectCommand({
    Bucket: env.S3_BUCKET_NAME,
    Key: key,
  }), {
    expiresIn: env.S3_PRESIGNED_URL_EXPIRATION_SECONDS,
  });
}

export async function deleteFile(key: string) {
  await s3.deleteObject({
    Bucket: env.S3_BUCKET_NAME,
    Key: key,
  });
}
