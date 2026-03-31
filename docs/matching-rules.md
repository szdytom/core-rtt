# Core RTT Match Making Rule Specification

## Scope

This document describes the rules on how should the server match players together in a Core RTT game. It covers the following topics:

- What strategy groups can be automatically matched together by the server.
- How the server computes the rating of a strategy group.

Simulating games locally is not the scope of this document. Friendly matches are not the scope of this document.

## Terms and Definitions

A *strategy* is a single valid ELF binary that can be executed by the Core RTT game engine. It works either on base runtime or on unit runtime. For strategy that works on base runtime, we call it a *base strategy*. For strategy that works on unit runtime, we call it a *unit strategy*.

A *strategy group* is a pair of a base strategy and a unit strategy. A strategy group has a unique identifier and uniquely computed rating.

A *tournament* is a finite state machine that consists of a set of strategy groups and a set of matches. Different tournaments may have different rules on how to select strategy groups for matches, how to compute the rating of a strategy group, and how to update the rating of a strategy group after a match.

*Dummy strategy* is a special strategy that does exactly nothing. Such strategy will never crash and will never invoke any game API.

*Dummy strategy group* is a strategy group that contains dummy strategies as both base strategy and unit strategy. Such strategy group will never win any match, it will only lose or draw.

*Session* is a period of time during which a strategy group can be automatically matched by the server.

## Division

The server divides all strategy groups into N divisions, from the weakest division (division N) to the strongest division (division 1). The parameter N is determined by the server.

In each session, the server will only match strategy groups within the same division. The server will start a tournament for each division. The session ends when all tournaments end. The next session starts after a fixed period of time determined by the server.

In each division tournament, the server will start a Swiss-system tournament. The server will select strategy groups in the division to play matches against each other. The top K strategy groups in the division will be promoted to the next stronger division, and the bottom K strategy groups in the division will be relegated to the next weaker division. The parameter K is determined by the server.

The rating of a strategy group shall be computed based on the results of matches in the division tournament.

## Qualification

A newly submitted strategy group must be qualified before it can be automatically matched by the server. Such qualification process must explicitly requested by the user. Once a qualification test is requested, the server will start a new *Qualification Tournament* for the strategy group. 

The Qualification Tournament consists the following stages:

- Qualify stage: The strategy group will be matched with the Dummy strategy group for 5 matches. If the strategy group wins at least 4 matches, it will be qualified. Otherwise, it will be disqualified ans the tournament ends.

- Division stage: For each division, from the weakest to the strongest, the strategy group will be matched with 2 randomly selected strategy groups in the division for 3 matches each. If the strategy group wins at least 2 matches against both of the selected strategy groups, it will be promoted to the next division. Otherwise, it will be qualified to start from the current division and the tournament ends. If the strategy group is promoted to the strongest division, it will be qualified to start from the strongest division and the tournament ends.

Once a strategy group is qualified, it can be automatically matched by the server starting from next session. The strategy group will be placed in the division where it is qualified to start.

## Rating System

The rating of a strategy group is computed based on the results of matches in the division tournament. The server will use a modified Glicko-2 rating system. The rating of a strategy group consists of two components: a rating score and a rating deviation.

The newly qualified strategy group will be assigned with an initial rating score of 1500 and an initial rating deviation of 350. The rating score represents the skill level of the strategy group, while the rating deviation represents the uncertainty of the rating score. The higher the rating deviation, the more uncertain the rating score is.

The leaderboard of the server will, however, rank strategy groups based on their rating score minus their rating deviation, instead of using their rating score directly. This is to encourage strategy groups with high uncertainty to play more matches and reduce their uncertainty.
