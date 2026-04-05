import { and, eq } from 'drizzle-orm';
import {
  decodePacket,
  encodePacket,
  MatchResult,
  type Packet,
  type SnowflakeId,
  TaskAcknowledgedPacket,
  TaskResultPacket,
  TaskStatus,
} from '@corertt/worker-codec';
import { db } from '~~/server/db/db';
import * as schema from '~~/server/db/schema';
import { getWorkerFromToken } from '~~/server/lib/jwt';
import { workerTaskBroker } from '~~/server/lib/workerTaskBroker';

const IDLE_MATCH_ID: SnowflakeId = '000000000000';

function parseBearerToken(authorizationHeader: string | null): string | null {
  if (!authorizationHeader)
    return null;

  return authorizationHeader.startsWith('Bearer ') ? authorizationHeader.replaceAll('Bearer ', '') : null;
}

function sendPacket(peer: { send: (data: unknown) => unknown }, packet: Packet): boolean {
  try {
    const payload = new Uint8Array(encodePacket(packet));
    peer.send(payload);
    return true;
  } catch {
    return false;
  }
}

function mapTaskResult(result: MatchResult): typeof schema.match.$inferInsert.result {
  switch (result) {
    case MatchResult.P1Win:
      return 'AWin';
    case MatchResult.P2Win:
      return 'BWin';
    case MatchResult.Tied:
      return 'draw';
    case MatchResult.NoResult:
      return 'no-result';
  }
}

function mapTaskStatus(status: TaskStatus): typeof schema.match.$inferInsert.status {
  switch (status) {
    case TaskStatus.Success:
      return 'finished';
    case TaskStatus.Polluted:
      return 'polluted';
    case TaskStatus.CoreCrash:
      return 'crashed';
  }
}

async function persistTaskResult(packet: TaskResultPacket): Promise<void> {
  await db.update(schema.match)
    .set({
      result: mapTaskResult(packet.result),
      status: mapTaskStatus(packet.status),
      workerLog: packet.errorLog,
      finishedAt: packet.finishedAt,
    })
    .where(eq(schema.match.id, packet.matchId));
}

async function markMatchRunning(matchId: SnowflakeId): Promise<void> {
  await db.update(schema.match)
    .set({
      status: 'running',
    })
    .where(and(
      eq(schema.match.id, matchId),
      eq(schema.match.status, 'pending'),
    ));
}

export default defineWebSocketHandler({
  async upgrade(request) {
    const token = parseBearerToken(request.headers.get('authorization'));
    if (!token) {
      return new Response('missing bearer token', { status: 401 });
    }

    let workerId: string | undefined;
    try {
      const tokenPayload = await getWorkerFromToken(token);
      workerId = tokenPayload.workerId;
    } catch {
      return new Response('invalid token', { status: 401 });
    }

    if (!workerId || typeof workerId !== 'string' || workerId.length === 0) {
      return new Response('invalid token payload', { status: 401 });
    }

    const worker = await db.query.worker.findFirst({
      where: and(
        eq(schema.worker.id, workerId),
        eq(schema.worker.status, 'normal'),
      ),
      columns: {
        id: true,
      },
    });

    if (!worker) {
      return new Response('worker not found', { status: 401 });
    }

    request.context.workerId = worker.id;
  },

  open(peer) {
    const workerId = peer.context.workerId as string;

    workerTaskBroker.registerConnection(peer.id, workerId, packet => sendPacket(peer, packet));
  },

  async message(peer, message) {
    const workerId = peer.context.workerId as string;

    let packet: Packet;
    try {
      packet = decodePacket(message.arrayBuffer() as ArrayBuffer);
    } catch (error) {
      console.error('[worker-communication] failed to decode packet', error);
      // Ignore invalid packets
      return;
    }

    if (packet instanceof TaskAcknowledgedPacket) {
      const acknowledged = workerTaskBroker.acknowledgeTask(peer.id, packet.matchId, packet.canAssignMore);
      if (!acknowledged && packet.matchId !== IDLE_MATCH_ID) {
        console.warn('[worker-communication] ignored unexpected task acknowledgement', {
          workerId,
          matchId: packet.matchId,
        });
        return;
      }

      if (acknowledged && packet.matchId !== IDLE_MATCH_ID) {
        await markMatchRunning(packet.matchId);
      }
      return;
    }

    if (packet instanceof TaskResultPacket) {
      if (!workerTaskBroker.isTaskAssignedToWorker(workerId, packet.matchId)) {
        console.warn('[worker-communication] rejected unexpected task result', {
          workerId,
          matchId: packet.matchId,
        });
        // Ignore results for tasks that were not acknowledged to be assigned to this worker
        return;
      }

      try {
        await persistTaskResult(packet);
      } catch (error) {
        console.error('[worker-communication] failed to persist task result', {
          workerId,
          matchId: packet.matchId,
          error,
        });

        return;
      }

      const completed = workerTaskBroker.completeTask(peer.id, workerId, packet.matchId);
      if (!completed) {
        console.error('[worker-communication] failed to complete task in broker after persisting result', {
          workerId,
          matchId: packet.matchId,
        });
      }

      return;
    }
  },

  close(peer) {
    workerTaskBroker.unregisterConnection(peer.id);
  },

  error(peer, error) {
    console.error('[worker-communication] websocket error', error);
    workerTaskBroker.unregisterConnection(peer.id);
  },
});
