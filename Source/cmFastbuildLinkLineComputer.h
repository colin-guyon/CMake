/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */

#ifndef cmFastbuildLinkLineComputer_h
#define cmFastbuildLinkLineComputer_h

#include "cmConfigure.h" // IWYU pragma: keep

#include "cmLinkLineComputer.h"

class cmComputeLinkInformation;
class cmOutputConverter;
class cmStateDirectory;

class cmFastbuildLinkLineComputer : public cmLinkLineComputer
{
  CM_DISABLE_COPY(cmFastbuildLinkLineComputer)

public:
  cmFastbuildLinkLineComputer(cmOutputConverter* outputConverter,
                              cmStateDirectory const& stateDir);

protected:
  std::string ComputeLinkLibs(cmComputeLinkInformation& cli) override;
};

#endif
