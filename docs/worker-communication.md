# Worker-Backend Communication Protocol

This document describes the communication protocol between the worker and the backend. The worker is responsible for executing tasks and running Core RTT games, while the backend manages task scheduling and coordination.

This document is meant to be read in conjunction with [Worker Codec Library](js/worker-codec/src/index.ts). The defined packets in the Worker Codec Library will not be repeated here.

## Backend API Endpoints

The backend exposes the following API endpoints for communication with the worker:

```
POST /api/worker/authenticate
WebSocket /api/worker/communication
```

### Authenticate Endpoint

The worker must authenticate with the backend by sending a POST request to the `/api/worker/authenticate` endpoint. The request should include the following JSON payload:

```json
{
  "worker_id": "unique_worker_id",
  "secret": "shared_secret"
}
```

The backend will validate the credentials and respond with a JSON object containing a JWT token if the authentication is successful

```json
{
  "token": "jwt_token"
}
```

## Authentication

The worker should be pre-configured with a unique worker ID and a shared secret that is known to the backend.

1. The worker sends a POST request to the `/api/worker/authenticate` endpoint with its credentials.
2. The backend validates the credentials and, if successful, responds with a JWT token.
3. The JWT token can be used for later authentication when establishing a WebSocket connection.

## WebSocket Communication

After successful authentication, the worker establishes a WebSocket connection to the `/api/worker/communication` endpoint. The request should include the JWT token in the `Authorization` header:

```
GET /api/worker/communication HTTP/1.1
Host: backend.example.com
Upgrade: websocket
Connection: Upgrade
Authorization: Bearer <jwt_token>
```

Once the WebSocket connection is established, the worker and the backend should send each other a `HelloPacket` to confirm the connection and codec version compatibility. The `HelloPacket` does not need to be responded to and its payload can be safely ignored. There is no need to wait for a `HelloPacket` before sending other packets, but it is recommended to send one as soon as possible after establishing the connection.

The worker should repeat `TaskAcknowledgedPacket` for each task assignment that was assigned to but unfinished by the worker in previous WebSocket connection. See the [Reconnection](#reconnection) section for more details.

When there is a task for the worker to execute, the backend will send a `TaskAssignmentPacket` containing the task details. The worker should acknowledge receipt of the task by sending a `TaskAcknowledgedPacket` back to the backend.

If the worker is able to accept more tasks, it should set the `canAssignMore` field of the `TaskAcknowledgedPacket` to `true`. If the worker is currently at capacity and cannot accept more tasks, it should set `canAssignMore` to `false`. This allows the backend to manage task scheduling more effectively by knowing which workers are available for new tasks.

If the backend does not receive a `TaskAcknowledgedPacket` within a reasonable time frame, it may assume that the worker is unresponsive and take appropriate action, such as closing the WebSocket connection and reassigning the task to another worker.

Once the worker has completed the assigned task, it should send a `TaskResultPacket` back to the backend with the results of the task execution. The backend will then process the results and update the task status accordingly.

If the backend does not receive a `TaskResultPacket` within a reasonable time frame after the worker has acknowledged the task, it may assume that the worker has failed to complete the task and take appropriate action, such as marking the task as timed out and failed.

It is worth noting that the backend should not consider a task as failed even if the websocket connection is lost before the worker can send the `TaskResultPacket`. The backend should wait for a reasonable amount of time for the worker to reconnect.

## Reconnection

If the WebSocket connection is lost, the worker should attempt to reconnect to the backend. Upon reconnection, the worker should authenticate again using the same process as described in the [Authentication](#authentication) section.

In the case of reconnection, the worker should also resend `TaskAcknowledgedPacket` for each task assignment that was assigned to but unfinished by the worker in previous WebSocket connection. This allows the backend to be aware of which tasks are still being worked on by the worker and can help prevent unnecessary reassignment of tasks to other workers.

Furthermore, these packets can inform the backend of the worker's current capacity to take on new tasks. If the worker is still at capacity, it can set `canAssignMore` to `false` in the `TaskAcknowledgedPacket`, which can help the backend make informed decisions about task scheduling and assignment.

## Task Assignment Availability

The backend should consider a connected worker as available for task assignment if

- the most recent `TaskAcknowledgedPacket` received from the worker has `canAssignMore` set to `true`, or
- the worker has not sent any `TaskAcknowledgedPacket` yet, but is connected for a reasonable amount of time without any message exchange.
- there is a more recent `TaskResultPacket` received from the worker than a `TaskAcknowledgedPacket`, which indicates that the worker has completed a task and is likely available for new tasks.

If the worker has received a `TaskAssignmentPacket` but it is at capacity and cannot accept the task immediately, it should still send a `TaskAcknowledgedPacket` with `canAssignMore` set to `false` to inform the backend of its current status. Furthermore, the worker should queue the task instead of rejecting or discarding it. Diagnostic logging is suggested in this case to help with debugging the backend's task scheduling and assignment logic.

If the worker has no task in progress for a reasonable amount of time, it can send a `TaskAcknowledgedPacket` with `matchId` set to `0` and `canAssignMore` set to `true` to proactively inform the backend of its availability for new tasks. However, after sending such packet, the worker should not send further `TaskAcknowledgedPacket` to save bandwidth.

## Storage Access

In the protocol, the worker may need to access storage resources such as downloading player's strategy files and uploading replay files. The authentication of these storage access should be fully transparent to the worker. The backend will provide pre-signed URLs for the worker to directly access the storage resources without needing to handle authentication tokens or credentials.
