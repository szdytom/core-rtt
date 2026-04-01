import { describe, expect, test } from 'vitest';
import { match, type GlickoPlayer } from '../src/index';
import { Glicko2, Player as RefPlayer } from 'glicko2';

function toOutcome(result: 'AWin' | 'BWin' | 'draw')  {
  if (result === 'AWin')
    return 1;
  if (result === 'BWin')
    return 0;
  return 0.5;
};

function expectClose(player: GlickoPlayer, reference: RefPlayer) {
  expect(player.rating).toBeCloseTo(reference.getRating(), 6);
  expect(player.ratingDeviation).toBeCloseTo(reference.getRd(), 6);
  expect(player.volatility).toBeCloseTo(reference.getVol(), 8);
};

describe('glicko', () => {
  test('matches glicko2js for sequential single-player updates', () => {
    const tau = 0.3;

    const initial: GlickoPlayer = {
      rating: 1500,
      ratingDeviation: 340,
      volatility: 0.06,
    };

    const system = new Glicko2({
      tau,
      rating: initial.rating,
      rd: initial.ratingDeviation,
      vol: initial.volatility,
    });

    let oursA = initial;
    let oursB = initial;

    const refA = system.makePlayer(initial.rating, initial.ratingDeviation, initial.volatility);
    const refB = system.makePlayer(initial.rating, initial.ratingDeviation, initial.volatility);

    const results: Array<'AWin' | 'BWin' | 'draw'> = [
      'AWin',
      'draw',
      'BWin',
      'AWin',
      'draw',
      'draw',
      'BWin',
      'AWin',
      'BWin',
      'BWin',
    ];

    for (const result of results) {
      const { newA, newB } = match(oursA, oursB, result, tau);
      oursA = newA;
      oursB = newB;

      system.updateRatings([[refA, refB, toOutcome(result)]]);

      expectClose(oursA, refA);
      expectClose(oursB, refB);
    }
  });

  test('matches glicko2js for sequential updates with multiple players', () => {
    const tau = 0.4;

    const players: Record<string, GlickoPlayer> = {
      A: { rating: 1500, ratingDeviation: 200, volatility: 0.06 },
      B: { rating: 1610, ratingDeviation: 80, volatility: 0.06 },
      C: { rating: 1420, ratingDeviation: 170, volatility: 0.06 },
      D: { rating: 1750, ratingDeviation: 60, volatility: 0.06 },
    };

    const schedule: Array<{ a: keyof typeof players, b: keyof typeof players, result: 'AWin' | 'BWin' | 'draw' }> = [
      { a: 'A', b: 'B', result: 'AWin' },
      { a: 'C', b: 'D', result: 'BWin' },
      { a: 'A', b: 'C', result: 'draw' },
      { a: 'B', b: 'D', result: 'BWin' },
      { a: 'A', b: 'D', result: 'BWin' },
      { a: 'B', b: 'C', result: 'AWin' },
      { a: 'C', b: 'D', result: 'draw' },
      { a: 'A', b: 'B', result: 'draw' },
      { a: 'D', b: 'B', result: 'AWin' },
      { a: 'C', b: 'A', result: 'BWin' },
    ];

    for (const game of schedule) {
      const oursA = players[game.a];
      const oursB = players[game.b];

      const { newA, newB } = match(oursA, oursB, game.result, tau);
      players[game.a] = newA;
      players[game.b] = newB;

      const system = new Glicko2({ tau });
      const refA = system.makePlayer(oursA.rating, oursA.ratingDeviation, oursA.volatility);
      const refB = system.makePlayer(oursB.rating, oursB.ratingDeviation, oursB.volatility);
      system.updateRatings([[refA, refB, toOutcome(game.result)]]);

      expectClose(players[game.a], refA);
      expectClose(players[game.b], refB);
    }
  });
});
