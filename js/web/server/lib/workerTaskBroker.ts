import type { SnowflakeId, TaskAssignPacket } from '@corertt/worker-codec';

const IDLE_MATCH_ID: SnowflakeId = '000000000000';

export interface WorkerConnection {
  connectionId: string;
  workerId: string;
  canAssignMore: boolean;
  sendTaskPacket: (packet: TaskAssignPacket) => boolean;
  unacknowledgedPackets: Map<SnowflakeId, TaskAssignPacket>;
}

export interface TaskDispatchEvent {
  matchId: SnowflakeId;
  workerId: string;
}

export interface TaskAcknowledgeEvent {
  matchId: SnowflakeId;
  workerId: string;
}

interface InFlightTaskOwnership {
  connectionId: string;
  workerId: string;
}

class WorkerTaskBroker {
  private readonly pendingQueue: TaskAssignPacket[] = [];
  private readonly pendingMatchIds = new Set<SnowflakeId>();
  private readonly connections = new Map<string, WorkerConnection>();
  private readonly inFlightMatchOwnership = new Map<SnowflakeId, InFlightTaskOwnership>();
  private readonly disabledWorkerIds = new Set<string>();

  public onTaskDispatched?: (event: TaskDispatchEvent) => void;
  public onTaskAcknowledged?: (event: TaskAcknowledgeEvent) => void;

  public enqueueTask(packet: TaskAssignPacket): void {
    if (this.isMatchTracked(packet.matchId)) {
      throw new Error(`match ${packet.matchId} is already queued or running`);
    }

    this.pendingQueue.push(packet);
    this.pendingMatchIds.add(packet.matchId);
    this.dispatchTasks();
  }

  public listConnections(): WorkerConnection[] {
    return Array.from(this.connections.values());
  }

  public setWorkerEnabled(workerId: string, enabled: boolean): void {
    if (enabled) {
      this.disabledWorkerIds.delete(workerId);
      this.dispatchTasks();
      return;
    }

    this.disabledWorkerIds.add(workerId);
    for (const connection of this.connections.values()) {
      if (connection.workerId === workerId) {
        connection.canAssignMore = false;
      }
    }
  }

  public registerConnection(connectionId: string, workerId: string, sendTaskPacket: (packet: TaskAssignPacket) => boolean): void {
    this.connections.set(connectionId, {
      connectionId,
      workerId,
      canAssignMore: !this.disabledWorkerIds.has(workerId),
      sendTaskPacket,
      unacknowledgedPackets: new Map<SnowflakeId, TaskAssignPacket>(),
    });

    this.dispatchTasks(connectionId);
  }

  public unregisterConnection(connectionId: string): void {
    const connection = this.connections.get(connectionId);
    if (connection == null) {
      return;
    }

    this.connections.delete(connectionId);

    this.requeueUnacknowledgedPackets(connection);

    this.dispatchTasks();
  }

  public acknowledgeTask(connectionId: string, matchId: SnowflakeId, canAssignMore: boolean): boolean {
    const connection = this.connections.get(connectionId);
    if (connection == null) {
      return false;
    }

    this.updateConnectionAvailability(connection, canAssignMore);

    if (matchId === IDLE_MATCH_ID) {
      if (canAssignMore) {
        this.dispatchTasks(connectionId);
      }
      return true;
    }

    const acknowledged = this.tryAcknowledgeTask(connection, connectionId, matchId);

    if (canAssignMore) {
      this.dispatchTasks(connectionId);
    }

    return acknowledged;
  }

  public isTaskAssignedToWorker(workerId: string, matchId: SnowflakeId): boolean {
    const ownership = this.inFlightMatchOwnership.get(matchId);
    return ownership != null && ownership.workerId === workerId;
  }

  public completeTask(connectionId: string, workerId: string, matchId: SnowflakeId): boolean {
    const ownership = this.inFlightMatchOwnership.get(matchId);
    if (ownership == null || ownership.workerId !== workerId) {
      return false;
    }

    this.inFlightMatchOwnership.delete(matchId);

    const targetConnection = this.resolveCompletionConnection(connectionId, workerId);

    if (targetConnection != null) {
      targetConnection.unacknowledgedPackets.delete(matchId);
      targetConnection.canAssignMore = true;
      this.dispatchTasks(targetConnection.connectionId);
      return true;
    }

    this.dispatchTasks();
    return true;
  }

  private dispatchTasks(preferredConnectionId?: string): void {
    if (this.pendingQueue.length === 0 || this.connections.size === 0) {
      return;
    }

    const orderedConnections = this.getOrderedConnections(preferredConnectionId);
    if (orderedConnections.length === 0) {
      return;
    }

    let madeProgress = true;
    while (madeProgress && this.pendingQueue.length > 0) {
      madeProgress = false;
      for (const connection of orderedConnections) {
        if (!this.canDispatchToConnection(connection)) {
          continue;
        }

        const packet = this.pendingQueue.shift();
        if (packet == null) {
          continue;
        }

        const didSend = this.dispatchPacketToConnection(connection, packet);
        if (!didSend) {
          continue;
        }

        madeProgress = true;
      }
    }
  }

  private getOrderedConnections(preferredConnectionId?: string): WorkerConnection[] {
    if (preferredConnectionId == null) {
      return Array.from(this.connections.values());
    }

    const preferredConnection = this.connections.get(preferredConnectionId);
    const others: WorkerConnection[] = [];
    for (const [connectionId, connection] of this.connections.entries()) {
      if (connectionId !== preferredConnectionId) {
        others.push(connection);
      }
    }

    if (preferredConnection == null) {
      return others;
    }

    return [preferredConnection, ...others];
  }

  private isMatchTracked(matchId: SnowflakeId): boolean {
    if (this.pendingMatchIds.has(matchId) || this.inFlightMatchOwnership.has(matchId)) {
      return true;
    }

    for (const connection of this.connections.values()) {
      if (connection.unacknowledgedPackets.has(matchId)) {
        return true;
      }
    }

    return false;
  }

  private requeueUnacknowledgedPackets(connection: WorkerConnection): void {
    const requeuePackets = Array.from(connection.unacknowledgedPackets.values());
    for (let index = requeuePackets.length - 1; index >= 0; index -= 1) {
      const packet = requeuePackets[index];
      if (packet == null) {
        continue;
      }
      if (this.isMatchTracked(packet.matchId)) {
        continue;
      }

      this.pendingQueue.unshift(packet);
      this.pendingMatchIds.add(packet.matchId);
    }
  }

  private updateConnectionAvailability(connection: WorkerConnection, canAssignMore: boolean): void {
    connection.canAssignMore = canAssignMore && !this.disabledWorkerIds.has(connection.workerId);
  }

  private tryAcknowledgeTask(connection: WorkerConnection, connectionId: string, matchId: SnowflakeId): boolean {
    const unacknowledgedPacket = connection.unacknowledgedPackets.get(matchId);
    if (unacknowledgedPacket != null) {
      connection.unacknowledgedPackets.delete(matchId);
      this.setInFlightMatchOwnership(matchId, connectionId, connection.workerId);
      this.onTaskAcknowledged?.({
        matchId,
        workerId: connection.workerId,
      });
      return true;
    }

    const existingOwnership = this.inFlightMatchOwnership.get(matchId);
    if (existingOwnership != null && existingOwnership.workerId === connection.workerId) {
      // Worker replayed unfinished acknowledgements after reconnect.
      this.setInFlightMatchOwnership(matchId, connectionId, connection.workerId);
      return true;
    }

    return false;
  }

  private setInFlightMatchOwnership(matchId: SnowflakeId, connectionId: string, workerId: string): void {
    this.inFlightMatchOwnership.set(matchId, {
      connectionId,
      workerId,
    });
  }

  private resolveCompletionConnection(connectionId: string, workerId: string): WorkerConnection | undefined {
    const directConnection = this.connections.get(connectionId);
    if (directConnection != null && directConnection.workerId === workerId) {
      return directConnection;
    }

    return this.findConnectionByWorkerId(workerId);
  }

  private canDispatchToConnection(connection: WorkerConnection): boolean {
    return !this.disabledWorkerIds.has(connection.workerId)
      && connection.canAssignMore
      && this.pendingQueue.length > 0;
  }

  private dispatchPacketToConnection(connection: WorkerConnection, packet: TaskAssignPacket): boolean {
    this.pendingMatchIds.delete(packet.matchId);
    connection.canAssignMore = false;
    connection.unacknowledgedPackets.set(packet.matchId, packet);

    const didSend = connection.sendTaskPacket(packet);
    if (!didSend) {
      connection.unacknowledgedPackets.delete(packet.matchId);
      this.pendingQueue.unshift(packet);
      this.pendingMatchIds.add(packet.matchId);
      return false;
    }

    this.onTaskDispatched?.({
      matchId: packet.matchId,
      workerId: connection.workerId,
    });
    return true;
  }

  private findConnectionByWorkerId(workerId: string): WorkerConnection | undefined {
    for (const connection of this.connections.values()) {
      if (connection.workerId === workerId) {
        return connection;
      }
    }

    return undefined;
  }
}

export const workerTaskBroker = new WorkerTaskBroker();
