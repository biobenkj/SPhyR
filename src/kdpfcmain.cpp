/*
 * dollomain.cpp
 *
 *  Created on: 9-mar-2018
 *      Author: M. El-Kebir
 */

#include <fstream>
#include <lemon/arg_parser.h>
#include "matrix.h"
#include "ilpsolverdolloflipcluster.h"
#include "coordinateascent.h"

int main(int argc, char** argv)
{
  std::string filename;
  int k = 1;
  double alpha = 1e-3;
  double beta = 0.3;
  int l = -1;
  bool exact = false;
  int seed = 0;
  int memoryLimit = -1;
  int nrThreads = 1;
  int timeLimit = -1;
  bool verbose = false;
  
  lemon::ArgParser ap(argc, argv);
  ap.refOption("k", "Maximum number of losses per SNV (default: 1)", k)
    .refOption("a", "False positive rate (default: 1e-3)", alpha)
    .refOption("b", "False negative rate (default: 0.3)", beta)
    .refOption("l", "Number of SNV clusters (default: -1, unlimited)", l)
    .refOption("exact", "Exact algorithm", exact)
    .refOption("s", "Random number generator seed (only applicable when used with -sA)", seed)
    .refOption("T", "Time limit in seconds (default: -1, unlimited)", timeLimit)
    .refOption("t", "Number of threads (default: 1)", nrThreads)
    .refOption("M", "Memory limit in MB (default: -1, unlimited)", memoryLimit)
    .refOption("v", "Verbose output", verbose)
    .other("input", "Input file")
    .other("output", "Output file");
  ap.parse();
  
  if (ap.files().empty())
  {
    std::cerr << "Error: missing input file" << std::endl;
    return 1;
  }
  
  std::ifstream inD(ap.files()[0]);
  if (!inD.good())
  {
    std::cerr << "Error: failed to open '" << argv[1] << "' for reading"
              << std::endl;
    return 1;
  }
  
  std::string outputFilename = ap.files().size() > 1 ? ap.files()[1] : "";
  
  Matrix D;
  inD >> D;
  inD.close();
  
  if (exact)
  {
    IlpSolverDolloFlipCluster solver(D, k, alpha, beta, l);
    solver.init();
    if (solver.solve(timeLimit, memoryLimit, nrThreads, verbose))
    {
      if (outputFilename.empty())
      {
        std::cout << solver.getSolE();
      }
      else
      {
        std::ofstream outE(outputFilename.c_str());
        outE << solver.getSolE();
        outE.close();
      }
    }
  }
  else
  {
    CoordinateAscent ca(D, k, alpha, beta, l, seed);
    ca.solve(timeLimit, memoryLimit, nrThreads, verbose);
    
    if (outputFilename.empty())
    {
      std::cout << ca.getE();
      std::cout << ca.getZ();
    }
    else
    {
      std::ofstream outFile(outputFilename.c_str());
      outFile << ca.getE();
      outFile << ca.getZ();
      outFile.close();
    }
  }
}