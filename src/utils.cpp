/*
 * utils.cpp
 *
 *  Created on: 22-feb-2018
 *      Author: M. El-Kebir
 */

#include "utils.h"

int g_lineNumber = 0;

lemon::Tolerance<double> g_tol(1e-4);

std::string getLineNumber()
{
  char buf[1024];
  
  snprintf(buf, 1024, "Line: %d. ", g_lineNumber);
  
  return std::string(buf);
}

std::istream& getline(std::istream& is, std::string& t)
{
  ++g_lineNumber;
  
  // source: http://stackoverflow.com/questions/6089231/getting-std-ifstream-to-handle-lf-cr-and-crlf
  t.clear();
  
  // The characters in the stream are read one-by-one using a std::streambuf.
  // That is faster than reading them one-by-one using the std::istream.
  // Code that uses streambuf this way must be guarded by a sentry object.
  // The sentry object performs various tasks,
  // such as thread synchronization and updating the stream state.
  
  std::istream::sentry se(is, true);
  std::streambuf* sb = is.rdbuf();
  
  for(;;) {
    int c = sb->sbumpc();
    switch (c) {
      case '\n':
        return is;
      case '\r':
        if(sb->sgetc() == '\n')
          sb->sbumpc();
        return is;
      case EOF:
        // Also handle the case when the last line has no line ending
        if(t.empty())
          is.setstate(std::ios::eofbit);
        return is;
      default:
        t += (char)c;
    }
  }
}

std::ostream& operator<<(std::ostream& out, const StlIntVector& z)
{
  out << z.size() << " #characters" << std::endl;
  
  bool first = true;
  for (int f : z)
  {
    if (first)
    {
      first = false;
    }
    else
    {
      out << " ";
    }
    out << f;
  }
  out << std::endl;
  
  return out;
}

std::istream& operator>>(std::istream& in, StlIntVector& z)
{
  int n = -1;
  std::string line;
  getline(in, line);
  std::stringstream ss(line);
  ss >> n;
  
  if (n < 0)
  {
    throw std::runtime_error(getLineNumber()
                             + "Error: number of characters should be positive.");
  }

  z = StlIntVector(n, -1);
  getline(in, line);
  ss.clear();
  ss.str(line);
  
  for (int c = 0; c < n; ++c)
  {
    ss >> z[c];
  }
  
  return in;
}
