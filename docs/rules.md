# Core RTT Game Rules Overview

This document provides an overview of the rules and mechanics of the Core RTT game. For detailed technical specifications, please refer 

- [Core RTT Rule Specification](./rule-spec.md)
- [Core RTT API Specification](./api-spec.md)

For any inconsistencies between this overview and the official specifications, the official specifications take precedence.

## Configurable Parameters (a.k.a Gamerules)

The game includes several configurable parameters that can be set by the referee program before the game starts. This document uses CAPTIALIZED NAMES to denote these parameters.

- **WIDTH**: The width of the battlefield (default: 64).
- **HEIGHT**: The height of the battlefield (default: 64).
- **BASE_SIZE**: The size of the base (default: 5).
- **UNIT_HEALTH**: The health points of a unit (default: 100).
- **NATURAL_ENERGY_RATE**: The number of turns required for a unit to naturally gain 1 energy (default: 1).
- **RESOURCE_ZONE_ENERGY_RATE**: The amount of energy a unit gains per turn when inside a resource zone (default: 25).
- **ATTACK_COOLDOWN**: The number of turns a unit must wait after attacking before it can attack again (default: 3).
- **CAPACITY_LV1**: The carry capacity of a unit before the capacity upgrade (default: 200).
- **CAPACITY_LV2**: The carry capacity of a unit after the capacity upgrade (default: 1000).
- **CAPACITY_UPGRADE_COST**: The energy cost for the capacity upgrade (default: 400).
- **VISION_UPGRADE_COST**: The energy cost for the vision upgrade (default: 1000).
- **DAMAGE_UPGRADE_COST**: The energy cost for the damage upgrade (default: 600).
- **MANUFACT_COST**: The energy cost for manufacturing a new unit (default: 500).
- **CAPTURE_TURN_THRESHOLD**: The number of consecutive turns required for a base to be captured (default: 8).

## Core Concepts

- **Battlefield**: A WIDTH×HEIGHT grid map. Terrain includes plain (passable), obstacle (impassable), and water (only bullets can pass).
- **Unit**: An entity controlled by a player (code) in the game. Each unit independently runs a program written by the player.
- **Bullet**: An entity in the game that causes damage upon collision with other entities.
- **Energy**: A resource used for attacking, repairing, upgrading, producing new robots, and other actions.
	The smallest unit of energy is 1, with a usable range of 0 to 65,535.

## Units

Each unit occupies one tile. A unit's position is always aligned with the grid. Two units cannot occupy the same tile.

- **Health**: UNIT_HEALTH points. A unit is destroyed when its health reaches zero.
- **Attack Frequency**: Can attack at most once every ATTACK_COOLDOWN turns, meaning it cannot attack for ATTACK_COOLDOWN - 1 turns after attacking.
- **Movement**: Can move one tile per turn (up/down/left/right). Cannot move onto water or obstacles.
- **Carry Capacity**: Can carry up to CAPACITY_LV1 energy (CAPACITY_LV2 after upgrade).
- **Vision**: Can see information (terrain, unit types, etc.) within a 5×5 area (9×9 after vision upgrade) centered on itself, limited to tiles within the same connected component of plain and water terrain.

Per NATURAL_ENERGY_RATE turns, a unit gains 1 energy. If a unit is on a resource zone, it gains RESOURCE_ZONE_ENERGY_RATE energy per turn in addition.

When attacking, pays x energy and fires a bullet with damage x (2x after upgrade). The bullet starts from the unit's position and moves 5 tiles per turn in the firing direction (up/down/left/right).

## Bullets

Each bullet occupies one tile. A bullet's position is always aligned with the grid. Two bullets cannot occupy the same tile.

- **Damage Value**: A positive integer, denoted as D.
- **Movement**: Moves 5 tiles per turn in its fired direction (up/down/left/right). Cannot pass through obstacles.
- **Collision**: When colliding with a unit of health x or a bullet of damage x:
    - If D > x, reduces the target's health/damage to zero and decreases its own damage by x.
    - If D <= x, removes itself and decreases the target's health/damage by D.

## Base

The base is a BASE_SIZE×BASE_SIZE area, and the terrain within this area is always plain (no obstacles or water).

- **Location**: Each side has one fixed base (immovable).
- **Functions**: Produce units, store energy, upgrade units, repair units.
- **Vision**: The BASE_SIZE×BASE_SIZE area.

Units within the base area can perform:
- **Energy Deposit/Withdrawal**: Deposit (part of) their carried energy into the base or withdraw (part of) the energy from the base.

The maximum energy the base can store is 65,535.

The base can pay MANUFACT_COST energy to manufacture a new unit. A player can own at most 15 units. The new unit spawns on a random empty tile within the base area.

The base can perform the following operations on units within its area:
- **Repair**: If a unit's current health is UNIT_HEALTH-x, pay 2x energy to restore its health to UNIT_HEALTH.
- **Upgrade**: Add one of the following upgrades to a unit. Upgrades are independent across different units and across different upgrades for the same unit. Each type of upgrade can only be performed once.
    - **Capacity Upgrade** (costs CAPACITY_UPGRADE_COST energy): Unit can carry up to CAPACITY_LV2 energy.
    - **Vision Upgrade** (costs VISION_UPGRADE_COST energy): Unit's visible area expands to 9×9.
    - **Damage Upgrade** (costs DAMAGE_UPGRADE_COST energy): The energy required for the unit to fire is halved.

If, for CAPTURE_TURN_THRESHOLD consecutive turns, the number of opponent units within a player's base area is strictly greater than the number of the player's own units within their own base area, the player's base is considered captured, and that player loses.

## Control

- The base runs an independent program, with instance ID 0.
- Each unit runs an independent instance, with instance IDs 1-15.
- **Communication Mechanism**:
    - Both the base and units can send messages to allies (512 bytes per packet), with a maximum of one message per turn.
    - Messages have a 1-turn delay, meaning they are received on the next turn.
    - Messages received in the current turn are lost if not read by the end of the turn.

## Initialization

- The terrain is randomly generated by the referee program, ensuring that plain tiles are connected.
- Each side initially spawns 3 units, numbered 1-3, on random tiles within their own base area.

### Turn Order

Within a turn, actions are strictly executed in the following order:

1. **Process Bullet Movement**: Repeated 5 times, each time moving all bullets 1 tile simultaneously and calculating collisions.
2. **Process Unit Movement**: All units move simultaneously.
3. **Process Base Capture Condition**
4. **Collect State and Execute Player Code**

## Interface Summary

The program can access the following interface to interact with the game environment:

- `meta(struct GameInfo* info)`: Retrieves game meta information (map size, base size, etc.).
- `turn()`: Returns the current turn number.
- `dev_info()`: Returns information about the current device (base or unit).
- `read_sensor(struct SensorData data[])`: Reads sensor data of surrounding tiles.
- `recv_msg(uint8_t buf[], int n)`: Receives a message from the queue.
- `send_msg(const uint8_t buf[], int n)`: Sends a message to all allied devices (once per turn).
- `pos()`: Returns the current coordinates (x, y) of the unit or base.

Base runtime only:
- `manufact(int id)`: Manufactures a new unit with the specified ID.
- `repair(int id)`: Repairs the unit with the specified ID.
- `upgrade(int id, int type)`: Upgrades the unit with the specified ID and upgrade type.

Unit runtime only:
- `fire(int direction, int power)`: Fires a bullet in the specified direction, consuming energy.
- `move(int direction)`: Moves the unit in the specified direction.
- `deposit(int amount)`: Deposits energy from the unit to the base.
- `withdraw(int amount)`: Withdraws energy from the base to the unit.
