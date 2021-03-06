/*
 * coordinateascent.cpp
 *
 *  Created on: 27-feb-2018
 *      Author: M. El-Kebir
 */

#include "coordinateascent.h"
#include "dkm/dkm.hpp"
//#include "ilpsolverdolloflipclustered.h"
#include "columngenflipclustered.h"
#include "cluster.h"

CoordinateAscent::CoordinateAscent(const Matrix& D,
                                   const StlIntVector& characterMapping,
                                   const StlIntVector& taxonMapping,
                                   int k,
                                   bool lazy,
                                   double alpha,
                                   double beta,
                                   int s,
                                   int t,
                                   int seed)
  : _D(D)
  , _k(k)
  , _lazy(lazy)
  , _alpha(alpha)
  , _beta(beta)
  , _s(std::min(s, D.getNrTaxa()))
  , _t(std::min(t, D.getNrCharacters()))
  , _seed(seed)
  , _E(s, t)
  , _zT(_D.getNrTaxa(), 0)
  , _zC(_D.getNrCharacters(), 0)
  , _L(0)
  , _baseL(0)
  , _restart(0)
{
  // Determine base likelihood based on fixed entries
  const double log_1_minus_alpha = log(1 - _alpha);
  const double log_1_minus_beta = log(1 - _beta);
  
  const int n = characterMapping.size();
  const int m = taxonMapping.size();
  int nrCorrectCharacters = 0;
  for (int c = 0; c < n; ++c)
  {
    if (characterMapping[c] == -1)
    {
      // all zeros (entire column is a TN)
      _baseL += log_1_minus_beta * m;
      ++nrCorrectCharacters;
    }
    else if (characterMapping[c] == -2)
    {
      // all ones (entire column is a TP)
      _baseL += log_1_minus_alpha * m;
      ++nrCorrectCharacters;
    }
    else if (characterMapping[c] <= -4)
    {
      // single one (TP) and m - 1 zeros (TN)
      _baseL += log_1_minus_beta * (m - 1) + log_1_minus_alpha;
      ++nrCorrectCharacters;
    }
  }
  
  int nrCorrectTaxa = 0;
  for (int p = 0; p < m; ++p)
  {
    if (taxonMapping[p] == -1)
    {
      // all zeros row
      // avoid double counting, hence n - nrCorrectCharacters
      _baseL += log_1_minus_beta * (n - nrCorrectCharacters);
      ++nrCorrectTaxa;
    }
  }
  
  std::cerr << "Number of fixed characters = " << nrCorrectCharacters << std::endl;
  std::cerr << "Number of fixed taxa = " << nrCorrectTaxa << std::endl;
  std::cerr << "Base log likelihood = " << _baseL << std::endl;
  
  const int mm = _D.getNrTaxa();
  const int nn = _D.getNrCharacters();
  
  StlIntMatrix invCharMapping(nn);
  for (int c = 0; c < n; ++c)
  {
    if (characterMapping[c] >= 0)
    {
      invCharMapping[characterMapping[c]].push_back(c);
    }
  }
  
  StlIntMatrix invTaxonMapping(mm);
  for (int p = 0; p < m; ++p)
  {
    if (taxonMapping[p] >= 0)
    {
      invTaxonMapping[taxonMapping[p]].push_back(p);
    }
  }
  
  _multiplicities = StlIntMatrix(_D.getNrTaxa(),
                                 StlIntVector(_D.getNrCharacters()));
  for (int pp = 0; pp < mm; ++pp)
  {
    const int nrTaxa = invTaxonMapping[pp].size();
    for (int cc = 0; cc < nn; ++cc)
    {
      const int nrCharacters = invCharMapping[cc].size();
      _multiplicities[pp][cc] = nrTaxa * nrCharacters;
    }
  }
}

void CoordinateAscent::initZ(int seed)
{
  Cluster cluster(_D, _s, _t);
  cluster.cluster(seed);
  _zT = cluster.getTaxonMapping();
  _zC = cluster.getCharacterMapping();
}

double CoordinateAscent::solveE(int timeLimit,
                                int memoryLimit,
                                int nrThreads,
                                bool verbose,
                                bool& success)
{
//  IlpSolverDolloFlipClustered solvePhylogeny(_D, _k, _alpha, _beta, _l, _z);
  ColumnGenFlipClustered solvePhylogeny(_D, _multiplicities, _baseL,
                                        _k, _lazy, _alpha, _beta,
                                        _t, _zC, _s, _zT);
  solvePhylogeny.init();
  if (_restart > 1)
  {
    solvePhylogeny.initHotStart(_E);
  }
  success = solvePhylogeny.solve(timeLimit, memoryLimit, nrThreads, verbose);
  
  _E = solvePhylogeny.getSolA();
#ifdef DEBUG
  InputMatrix::ViolationList violationList;
  assert(_E.identifyViolations(_k, violationList));
  assert(violationList.empty());
#endif // DEBUG
  
  return computeLogLikelihood();
}

double CoordinateAscent::computeCharacterLogLikelihood(int c, int f) const
{
  const int m = _D.getNrTaxa();
  const int n = _D.getNrCharacters();
  
  assert(0 <= c && c < n);
  assert(0 <= f && f < _t);
  
  double L = 0;
  for (int p = 0; p < m; ++p)
  {
    int h = _zT[p];
    L += computeLogLikelihood(p, h, c, f);
  }
  return L;
}

double CoordinateAscent::computeTaxonLogLikelihood(int p, int h) const
{
  const int m = _D.getNrTaxa();
  const int n = _D.getNrCharacters();
  
  assert(0 <= p && p < m);
  assert(0 <= h && h < _s);
  
  double L = 0;
  for (int c = 0; c < n; ++c)
  {
    int f = _zC[c];
    L += computeLogLikelihood(p, h, c, f);
  }
  return L;
}

double CoordinateAscent::computeLogLikelihood(int p, int h,
                                              int c, int f) const
{
  const int m = _D.getNrTaxa();
  const int n = _D.getNrCharacters();
  
  assert(0 <= c && c < n);
  assert(0 <= f && f < _t);
  
  assert(0 <= p && p < m);
  assert(0 <= h && h < _s);
  
  const double log_alpha = log(_alpha);
  const double log_1_minus_alpha = log(1 - _alpha);
  const double log_beta = log(_beta);
  const double log_1_minus_beta = log(1 - _beta);
  
  int d_pc = _D.getEntry(p, c);
  int a_hf = _E.getEntry(h, f);
  assert(d_pc == 0 || d_pc == 1 || d_pc == -1);
  
  int mult = _multiplicities[p][c];
  
  if (d_pc == 1)
  {
    if (a_hf == 1)
    {
      return mult * log_1_minus_alpha;
    }
    else
    {
      return mult * log_alpha;
    }
  }
  else if (d_pc == 0)
  {
    if (a_hf == 1)
    {
      return mult * log_beta;
    }
    else
    {
      return mult * log_1_minus_beta;
    }
  }
  else
  {
    return 0;
  }
}

double CoordinateAscent::computeLogLikelihood() const
{
  const int m = _D.getNrTaxa();
  const int n = _D.getNrCharacters();
  
  double L = _baseL;
  
  for (int p = 0; p < m; ++p)
  {
    const int h = _zT[p];
    for (int c = 0; c < n; ++c)
    {
      const int f = _zC[c];
      L += computeLogLikelihood(p, h, c, f);
    }
  }
  
//  const double log_gamma = log(_gamma);
//  for (int f = 0; f < _lC; ++f)
//  {
//    for (int i = 2; i <= _k + 1; ++i)
//    {
//      for (int h = 0; h < _lT; ++h)
//      {
//        if (_E.getEntry(h, f) == i)
//        {
//          L += log_gamma;
//          break;
//        }
//      }
//    }
//  }
  
  return L;
}

double CoordinateAscent::solveZC()
{
  const int n = _D.getNrCharacters();
  
  double L = _baseL;
  for (int c = 0; c < n; ++c)
  {
    double maxL_c = -std::numeric_limits<double>::max();
    int max_f = -1;
    for (int f = 0; f < _t; ++f)
    {
      double L_cf = computeCharacterLogLikelihood(c, f);
      if (L_cf > maxL_c)
      {
        maxL_c = L_cf;
        max_f = f;
      }
    }
    
    assert(max_f != -1);
    _zC[c] = max_f;
    L += maxL_c;
  }
  
  return L;
}

double CoordinateAscent::solveZT()
{
  const int m = _D.getNrTaxa();
  
  double L = _baseL;
  for (int p = 0; p < m; ++p)
  {
    double maxL_p = -std::numeric_limits<double>::max();
    int max_h = -1;
    for (int h = 0; h < _s; ++h)
    {
      double L_ph = computeTaxonLogLikelihood(p, h);
      if (L_ph > maxL_p)
      {
        maxL_p = L_ph;
        max_h = h;
      }
    }
    
    assert(max_h != -1);
    _zT[p] = max_h;
    L += maxL_p;
  }
  
  return L;
}

bool CoordinateAscent::solve(int timeLimit,
                             int memoryLimit,
                             int nrThreads,
                             bool verbose,
                             int nrRestarts)
{
  // MEK: limit maximum number of iterations in one restart
  const int maxIterations = 100;
  
  Matrix bestA(_D.getNrTaxa(), _E.getNrCharacters());
  double bestLikelihood = computeLogLikelihood();
  _L = bestLikelihood;
  StlIntVector bestZT = _zT, bestZC = _zC;
  
  bool timeLeft = true;
  for (_restart = 1; _restart <= nrRestarts && timeLeft; ++_restart)
  {
    initZ(_seed + _restart - 1);
    
    double delta = 1;
    int iteration = 1;
    double L = -std::numeric_limits<double>::max();
    while (g_tol.nonZero(delta) && iteration <= maxIterations && timeLeft)
    {
      double LLL = solveE(timeLimit, memoryLimit, nrThreads, verbose, timeLeft);
      std::cerr << "Restart " << _restart << " -- iteration " << iteration << " -- E step -- log likelihood " << LLL << std::endl;
//      std::cout << _E << std::endl;
      assert(!timeLeft || !g_tol.less(LLL, L));
      
      double LL = solveZT();
      std::cerr << "Restart " << _restart << " -- iteration " << iteration << " -- zT step -- log likelihood " << LL << std::endl;
      double newL = solveZC();
      std::cerr << "Restart " << _restart << " -- iteration " << iteration << " -- zC step -- log likelihood " << newL << std::endl;
//      std::cout << _E << std::endl;
      std::cerr << std::endl;
      
      delta = newL - L;
      _L = newL;
      L = newL;
      ++iteration;
    }
    
    if (bestLikelihood < _L)
    {
      bestA = _E;
      bestLikelihood = _L;
      bestZT = _zT;
      bestZC = _zC;
    }
  }
  
  _E = bestA;
  _L = bestLikelihood;
  _zT = bestZT;
  _zC = bestZC;
  
  return true;
}
