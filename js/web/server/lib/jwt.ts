import * as jose from 'jose';
import { nanoid } from 'nanoid';
import { env } from '../env';

export async function produceAccessToken(id: string) {
  const encPublicKey = await jose.importSPKI(env.ENC_PUBLIC_KEY, 'RSA-OAEP-256');
  const signPrivateKey = await jose.importPKCS8(env.SIGN_PRIVATE_KEY, 'RS512');

  const jwt = await new jose.SignJWT({})
    .setSubject(id.toString())
    .setIssuedAt()
    .setExpirationTime(env.TOKEN_EXPIRATION_TIME)
    .setIssuer('core-rtt')
    .setJti(nanoid(32))
    .setProtectedHeader({
      alg: 'RS512',
      kid: env.SIGN_KID,
    })
    .sign(signPrivateKey);

  const jwe = await new jose.CompactEncrypt(new TextEncoder().encode(jwt))
    .setProtectedHeader({
      alg: 'RSA-OAEP-256',
      enc: 'A256GCM',
      kid: env.ENC_KID,
    })
    .encrypt(encPublicKey);

  return jwe;
}

export async function getWorkerFromToken(token: string) {
  const signPublicKey = await jose.importSPKI(env.SIGN_PUBLIC_KEY, 'RS512');
  const encPrivateKey = await jose.importPKCS8(env.ENC_PRIVATE_KEY, 'RSA-OAEP-256');

  const { plaintext: decryptedJwt } = await jose.compactDecrypt(token, encPrivateKey);
  const { payload } = await jose.jwtVerify(new TextDecoder().decode(decryptedJwt), signPublicKey, {
    algorithms: ['RS512'],
    issuer: 'core-rtt',
  });

  const workerId = payload.sub;
  if (typeof workerId !== 'string' || workerId.length === 0)
    throw new Error('invalid token subject');

  return { workerId };
}
