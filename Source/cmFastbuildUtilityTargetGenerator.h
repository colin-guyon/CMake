/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#ifndef cmFastbuildUtilityTargetGenerator_h
#define cmFastbuildUtilityTargetGenerator_h

#include <cmFastbuildTargetGenerator.h>

class cmFastbuildUtilityTargetGenerator : public cmFastbuildTargetGenerator
{
public:
  cmFastbuildUtilityTargetGenerator(cmGeneratorTarget* gt);

  virtual void Generate();
};

#endif // cmFastbuildUtilityTargetGenerator_h
