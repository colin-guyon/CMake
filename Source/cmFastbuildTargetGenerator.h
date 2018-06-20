/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#ifndef cmFastbuildTargetGenerator_h
#define cmFastbuildTargetGenerator_h

#include <cmCommonTargetGenerator.h>

class cmFastbuildTargetGenerator : public cmCommonTargetGenerator
{
public:
  cmFastbuildTargetGenerator(cmGeneratorTarget* gt);

  virtual void Generate() = 0;

  virtual void AddIncludeFlags(std::string& flags, std::string const& lang);

  std::string ConvertToFastbuildPath(const std::string& path);
};

#endif // cmFastbuildTargetGenerator_h
