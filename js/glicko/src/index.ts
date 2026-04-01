function g(phi: number) {
  return 1 / Math.sqrt(1 + (3 * phi ** 2) / (Math.PI ** 2));
}

function E(mu: number, muB: number, phiB: number) {
  return 1 / (1 + Math.exp(-g(phiB) * (mu - muB)));
}

export type GlickoPlayer = {
  rating: number;
  ratingDeviation: number;
  volatility: number;
};

function update(
  player: GlickoPlayer,
  opponent: GlickoPlayer,
  result: number, // 1 for win, 0 for loss, 0.5 for draw
  tau: number = 0.3,
) {
  // Step 1: Define constants
  const scalar = 173.7178; // Scalar to convert between Glicko-2 scale and rating scale

  // Step 2: Convert ratings to Glicko-2 scale
  const muA = (player.rating - 1500) / scalar;
  const phiA = player.ratingDeviation / scalar;
  const sigA = player.volatility;

  const muB = (opponent.rating - 1500) / scalar;
  const phiB = opponent.ratingDeviation / scalar;

  // Step 3: Calculate the expected score
  const v = 1 / (g(phiB) ** 2 * E(muA, muB, phiB) * (1 - E(muA, muB, phiB)));

  // Step 4: Update volatility
  const Delta = v * g(phiB) * (result - E(muA, muB, phiB));

  // Step 5: Determine the new volatility
  const a = Math.log(sigA ** 2);
  const epsilon = 0.000001; // 1e-5

  const f = (x: number) => {
    const ex = Math.exp(x);
    const num = ex * (Delta ** 2 - phiA ** 2 - v - ex);
    const den = 2 * (phiA ** 2 + v + ex) ** 2;
    return (num / den) - ((x - a) / (tau ** 2));
  };

  // Step 5.2
  let A = a;
  let B = 0;
  if (Delta ** 2 > phiA ** 2 + v) {
    B = Math.log(Delta ** 2 - phiA ** 2 - v);
  } else {
    let k = 1;
    while (f(a - k * tau) < 0) {
      k++;
    }

    B = a - k * tau;
  }

  // Step 5.3~5.4
  let fA = f(A);
  let fB = f(B);
  while (Math.abs(B - A) > epsilon) {
    const C = A + (A - B) * fA / (fB - fA);
    const fC = f(C);

    if (fC * fB <= 0) {
      A = B;
      fA = fB;
    } else {
      fA /= 2;
    }

    B = C;
    fB = fC;
  }

  // Step 5.5
  const sigPrime = Math.exp(A / 2);

  // Step 6: Update rating deviation
  const phiStar = Math.sqrt(phiA ** 2 + sigPrime ** 2);

  // Step 7: Update rating
  const phiPrime = 1 / Math.sqrt((1 / phiStar ** 2) + (1 / v));
  const muPrime = muA + (phiPrime ** 2 * g(phiB) * (result - E(muA, muB, phiB)));

  // Step 8: Convert back to original scale
  const newRatingA = muPrime * scalar + 1500;
  const newRatingDeviationA = phiPrime * scalar;
  const newVolatilityA = sigPrime;

  return {
    rating: newRatingA,
    ratingDeviation: newRatingDeviationA,
    volatility: newVolatilityA,
  };
}

export function match(
  playerA: GlickoPlayer,
  playerB: GlickoPlayer,
  result: 'AWin' | 'BWin' | 'draw',
  tau: number = 0.3,
) {
  const newA = update(playerA, playerB, result === 'AWin' ? 1 : result === 'draw' ? 0.5 : 0, tau);
  const newB = update(playerB, playerA, result === 'BWin' ? 1 : result === 'draw' ? 0.5 : 0, tau);

  return { newA, newB };
}
