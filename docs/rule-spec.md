# Core RTT Rule Specification

- Version 0.2

## Scope

This document defines the official rules, mechanics, and constraints governing the Core RTT game. The rules outlined here are authoritative and must be strictly followed by all participants. How should players implement their strategies and interact with the game environment is out of scope for this document and is instead covered in the [Core RTT API Specification](./api-spec.md).

## Definitions and Terminology

A _player_ is one of the two sides in the game. It refers to the human or team controlling the code that runs the units in the game.

A _strategy_ is the algorithm or logic implemented by a player's code to control their base or units in the game. A _strategy group_ is a pair of a strategy for the base and a strategy for the units.

_Turn_ is the fundamental time unit in the game. A turn comprises a sequence of actions and state updates.

_Tile_ is the basic spatial unit in the game, representing a single square on the grid-based battlefield, identified by its (x, y) coordinates. The _battlefield_ is a grid of WIDTH×HEIGHT tiles.

A tile can be one of three types of terrain: _plain_ (passable), _obstacle_ (impassable), and _water_ (only bullets can pass).

An _entity_ is any object in the game that occupies a tile, more specifically, a unit or a bullet.

_Energy_ is a only resource in the game, used for attacking, repairing, upgrading, producing new robots, and other actions. The smallest unit of energy is 1, with a usable range of 0 to 65,535.

## Configurable Parameters

The game includes several configurable parameters that can be set by the referee program before the game starts. This document uses CAPITALIZED NAMES to denote these parameters. The configurable parameters are sometime informally referred to as gamerules.

| Parameter Name | Description | Default Value |
| --- | --- | --- |
| WIDTH | The width of the battlefield. | 64 |
| HEIGHT | The height of the battlefield. | 64 |
| BASE_SIZE | The size of the base. | 5 |
| UNIT_HEALTH | The initial health points of a unit. | 100 |
| NATURAL_ENERGY_RATE | The number of turns required for a unit to naturally gain 1 energy. | 1 |
| RESOURCE_ZONE_ENERGY_RATE | The amount of energy a unit gains per turn when inside a resource zone. | 25 |
| ATTACK_COOLDOWN | The number of turns a unit must wait after attacking before it can attack again. | 3 |
| CAPACITY_LV1 | The carry capacity of a unit before the capacity upgrade. | 200 |
| CAPACITY_LV2 | The carry capacity of a unit after the capacity upgrade. | 1000 |
| VISION_LV1 | The vision range of a unit before the vision upgrade. | 5 |
| VISION_LV2 | The vision range of a unit after the vision upgrade. | 9 |
| CAPACITY_UPGRADE_COST | The energy cost for the capacity upgrade. | 400 |
| VISION_UPGRADE_COST | The energy cost for the vision upgrade. | 1000 |
| DAMAGE_UPGRADE_COST | The energy cost for the damage upgrade. | 600 |
| MANUFACT_COST | The energy cost for manufacturing a new unit. | 500 |
| CAPTURE_TURN_THRESHOLD | The number of consecutive turns required for a base to be captured. | 8 |

## Core Concepts

### Resource Zone

A tile can be designated as a resource zone, where units can gain energy at a higher rate. The amount of energy gained per turn in a resource zone is defined by the RESOURCE_ZONE_ENERGY_RATE parameter. The terrain type of a tile is independent of whether it is a resource zone.

### Battlefield

The battlefield is a WIDTH×HEIGHT grid map. Coordinates are represented as (x, y), where x is the horizontal coordinate (0 ≤ x < WIDTH) and y is the vertical coordinate (0 ≤ y < HEIGHT). The coordinate system's origin (0, 0) is located at the top-left corner of the battlefield, with x increasing to the right and y increasing downwards (Don't confuse it with Cartesian coordinate system or row-column indexing).

### Unit

A unit is an entity controlled by a strategy in the game. Each unit independently runs a program written by the player. Units can perform various actions such as moving, attacking, and carrying energy.

A unit occupies a single plain tile and cannot move onto water or obstacle tiles. The occupation is exclusive, meaning two units cannot occupy the same tile simultaneously, nor can a unit occupy a tile with a bullet.

A property known as _health_ is associated with each unit. The initial health of a unit is defined by the UNIT_HEALTH parameter. A unit is destroyed and removed from the game when its health reaches zero.

Unit can carry energy, which is used for various actions. The maximum amount of energy a unit can carry is defined by the CAPACITY_LV1 parameter, which can be increased to CAPACITY_LV2 through the _capacity upgrade_.

Energy can be gained naturally over time or by being in a resource zone. Whenever the number of turns that have passed is a multiple of NATURAL_ENERGY_RATE, each unit gains 1 energy. Additionally, if a unit is on a resource zone, it gains RESOURCE_ZONE_ENERGY_RATE energy per turn. The energy gained is saturation-limited by the unit's carry capacity.

The vision of a unit is defined as the area around it. By default, a unit can see information (terrain, unit types, etc.) within a 5×5 area centered on itself, limited to tiles within the same connected component of plain and water terrain, and obstacles tiles that are orthogonally adjacent to any tile within that connected component (and within vision range). The vision range can be increased to 9×9 through the _vision upgrade_.

For example, for the following 5×5 area centered on a unit (unit denoted as `@`, obstacle as `#`, plain as `.`, and water as `~`):

```
. # . # .
# . . ~ ~
. . @ # .
# . # # #
. # # . .
```

The unit's vision includes the following tiles (invisible tiles are denoted as `?`):

```
? # . # .
# . . ~ ~
. . @ # .
# . # ? #
? # ? ? ?
```

When a unit is near the edge of the battlefield, the out-of-bounds area is considered invisible. For example,

```
. # ~ ~ .
. @ # . .
. # . # .
. # ~ # . 
. . . . .
```

The unit's vision includes the following tiles:

```
? ? ? ? ?
? . # ? ?
? . @ # ?
? . # ? ?
? . # ? ?
```

A unit can perform an attack action to fire a bullet in one of the four cardinal directions (up, down, left, right). To perform an attack, the unit must pay a certain amount of energy, decided by the strategy, which determines the damage value of the bullet. If the amount of energy paid is x, the bullet will have a damage value of x (or 2x if the unit has the _damage upgrade_).

The unit can only attack once per ATTACK_COOLDOWN turns. After a unit attacks, it cannot attack again for ATTACK_COOLDOWN - 1 turns. For example, if ATTACK_COOLDOWN is 3 and a unit attacks at turn t, it cannot attack at turns t+1 and t+2, but can attack again at turn t+3.

A unit can move one tile orthogonally (up, down, left, right) per turn. A unit cannot move onto water or obstacle tiles. If a unit attempts to move onto an impassable tile, the move action fails and the unit remains in its current position. See the [Unit Movement Resolution](#unit-movement-resolution) section for details on how movement conflicts are resolved.

Units can be upgraded at the base to enhance their capabilities. The available upgrades include:

- **Capacity Upgrade**: Increases the unit's carry capacity from CAPACITY_LV1 to CAPACITY_LV2 (costs CAPACITY_UPGRADE_COST energy).
- **Vision Upgrade**: Expands the unit's visible area from 5×5 to 9×9 (costs VISION_UPGRADE_COST energy).
- **Damage Upgrade**: Halves the energy required for the unit to fire a bullet (costs DAMAGE_UPGRADE_COST energy).

These upgrades are independent across different units and across different upgrades for the same unit. Each type of upgrade can only be performed once per unit.

A player can own at most 15 units, indexed from 1 to 15. A player cannot manufacture a new unit if they already own 15 units.

### Bullet

A bullet is an entity created by a unit's attack action. Each bullet occupies a single tile and cannot occupy the same tile as another bullet or a unit.

A damage value is associated with each bullet, determined by the amount of energy paid by the unit when firing it. The bullet moves 5 tiles per turn in the direction it was fired (up, down, left, right). A bullet cannot pass through obstacle tiles but can pass through plain and water tiles.

A bullet will move orthogonally along its direction of fire. Each turn, the bullet moves 5 tiles in that direction. If the bullet encounters an obstacle tile or the edge of the battlefield, it will stop and be removed from the game.

When a bullet collides with a unit or another bullet, the following rules apply:
- If a bullet with damage D collides with a unit of health x:
	- If D > x, the unit's health is reduced to zero (destroyed) and the bullet's damage is reduced by x.
	- If D <= x, the bullet is removed from the game and the unit's health is reduced by D.
- If a bullet with damage D collides with another bullet of damage x:
	- If D > x, the second bullet is removed from the game and the first bullet's damage is reduced by x.
	- If D <= x, the first bullet is removed from the game and the second bullet's damage is reduced by D.

### Base

The base is a fixed, immovable rectangular area of size BASE_SIZE×BASE_SIZE on the battlefield. The terrain within the base area is always plain, meaning there are no obstacles or water tiles within the base. The base tiles are independent of the resource zones, meaning a tile within the base can also be a resource zone, while usually not.

The base's position is defined as the top-left tile of the BASE_SIZE×BASE_SIZE area. Each player has one base located at a fixed position.

The base can store energy. The maximum energy the base can store is 65,535. Units can deposit energy into the base or withdraw energy from the base when they are within the base area. The base does not produce energy on its own.

The base's vision includes the entire BASE_SIZE×BASE_SIZE area.

The base can perform the following operations on units within its area:

- **Energy Deposit/Withdrawal**: A unit can deposit (part of) its carried energy into the base or withdraw (part of) the energy from the base. The amount of energy deposited or withdrawn is decided by the strategy.
- **Manufacture**: The base can pay MANUFACT_COST energy to manufacture a new unit. A player can own at most 15 units. The new unit spawns on a random empty tile within the base area.
- **Repair**: If a unit's current health is UNIT_HEALTH-x, the base can pay 2x energy to restore the unit's health to UNIT_HEALTH.
- **Upgrade**: The base can add one of the following upgrades to a unit. Refer to [Units](#unit) section for details on the upgrades.

BASE_SIZE should be at least 2 and at most 8.

## End Game Condition

A player's base is in _capture condition_ if the number of opponent units within the player's base area is strictly greater than the number of the player's own units within their own base area. A player's base is considered _captured_ if for CAPTURE_TURN_THRESHOLD consecutive turns, the player's base is in capture condition. If a player's base is captured, that player loses.

If both players' bases are captured in the same turn, the game continues until one player's base is no longer in capture condition, but the other player's base is still in capture condition. If both players' bases exit capture condition in the same turn, the game continues.

If both players' bases have insufficient energy to manufacture new units and all existing units are destroyed, the game ends in a draw immediately.

There is a turn limit for draw. This limit is implementation-defined and can be dynamically adjusted by the referee program based on the game state. It is required that such limit is at least 1,000 turns.

Further losing conditions to panelize error-prone strategies can be defined, such as programs failed to load or crashed multiple times, but they are implementation-defined and not required. The implementation can just kindly reload/restart the program without penalizing the player, or can choose to penalize the player after a certain number of reloads/restarts.

A possible draw condition can be defined as follows:

- A `max_turn` counter is initialized to 1,000 at the start of the game.
- For the first occurrence of each of the following events independently, `max_turn` is increased by 500:
	- A unit moves and left the base area.
	- A unit entered a resource zone.
- For the first occurrence of each of the following events independently, `max_turn` is increased by 2,000:
	- A unit is upgraded or non-trivially (i.e., at least repaired 1 health) repaired at the base.
	- A unit is manufactured.
	- A unit is destroyed.
	- A base enters capture condition.
- If the turn number reaches `max_turn`, the game ends in a draw.

Using such dynamic turn limit can encourage players to be more aggressive and prevent excessively long games. It is easy to find out that the maximum possible value of `max_turn` is 10,000, which means the game will end in a draw if it reaches turn 10,000, no matter what happens.

Please note that above is just an example of a possible draw condition. The actual draw condition and turn limit are implementation-defined and may differ from the example provided.

## Unit Movement Resolution

When multiple units attempt to move onto the same tile, a movement conflict occurs. To address these conflicts, the game uses the following resolution rules:

We say a unit's move is _cancelled_ if it's move will not be executed. The _target tile_ of a unit's move is the tile that the unit attempts to move onto. For a unit's intended move:

+ If the _target tile_ is a water or obstacle tile, the move is cancelled.
+ If the _target tile_ is occupied by a bullet, the move is cancelled.
+ If another unit's intended move has the same _target tile_, both moves are cancelled.
+ If the _target tile_ is occupied by another unit, the move is cancelled if:
	+ The occupying unit's has no intended move this turn, or
	+ The occupying unit's intended move is to move against the first unit's current position, or
	+ The occupying unit's intended move is cancelled.
+ Otherwise, the move is successful.

## Control and Communication

Base program and unit programs are independent. The visible effect of all actions from the base program will be processed before the unit programs. The order of execution for base and unit programs are implementation-defined, but there should be no observable difference for players, as if all programs are executed concurrently.

Each program (base or unit) has a unique instance ID. The base program has an instance ID of 0, while unit programs have instance IDs from 1 to 15, corresponding to the units they control.

Any program can send a message to all programs of the same player (including itself) using the communication mechanism provided by the game. The message will be delivered to the target program at the beginning of the next turn after it is sent. The content and format of the message are decided by the strategy and are opaque to the game engine. Each program can send at most one message per turn, and each message can be at most 512 bytes in size. Messages that are received in the current turn but not read by the end of the turn will be lost.

That is, if a program sends a message at turn t, the message can be read by the programs of the same player at turn t+1. If a program does not read the message at turn t+1, the message will be lost and cannot be read in subsequent turns.

The messages are broadcast to all programs of the same player, meaning that if a program sends a message, all programs of the same player, including the sender itself, can read the message at the beginning of the next turn. There is no direct way to send a message to a specific program, but a program can include the intended recipient's instance ID in the message content to achieve a similar effect.

## Turn Structure

Each turn consists of a rigidly ordered sequence of steps. All actions within a step are processed simultaneously based on the state at the start of the step, unless otherwise specified.

+ **Turn Start**: The turn begins, the turn number is incremented.
+ **Process Bullet Movement**: This step is repeated 5 times:
	+ All bullets move one tile in their direction.
	+ Collisions are processed for all new tile occupations, including bullet-unit collisions and bullet-bullet collisions, following the rules defined in the [Bullet](#bullet) section.
	+ Bullets destroyed in this sub-step are removed from the game before the next sub-step.
+ **Process Unit Movement**: All units attempt to move simultaneously based on their intended moves. Movement conflicts are resolved according to the rules defined in the [Unit Movement Resolution](#unit-movement-resolution) section.
+ **End Game Check**: The game engine checks if any end game condition is met (e.g., base capture, draw condition) and ends the game if necessary.
+ **Process Unit Energy Gain**: Each unit gains energy according to the rules defined in the [Unit](#unit) section.
+ **Process Base Actions**: The base program executes its actions (e.g., manufacturing, repairing, upgrading).
+ **Process Unit Actions**: All unit programs execute their actions (e.g., moving, attacking, depositing/withdrawing energy).

## Game Initialization

+ The referee program loads a preset map or generates a random map, ensuring that two bases are connected by plain tiles.
+ Each player starts with three units in their base area, spawned on random empty tiles within the base area. The units are indexed from 1 to 3.
+ All units start with UNIT_HEALTH health, 0 energy, and no upgrades.
+ The base starts with 0 energy.
+ The turn number is initialized to 0.
