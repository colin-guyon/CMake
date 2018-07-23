/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */

#include "cmFastbuildLinkLineComputer.h"

#include "cmComputeLinkInformation.h"
#include "cmGeneratorTarget.h"

cmFastbuildLinkLineComputer::cmFastbuildLinkLineComputer(
  cmOutputConverter* outputConverter, cmStateDirectory const& stateDir)
  : cmLinkLineComputer(outputConverter, stateDir)
{
}

std::string cmFastbuildLinkLineComputer::ComputeLinkLibs(cmComputeLinkInformation& cli)
{
  std::string linkLibs;
  typedef cmComputeLinkInformation::ItemVector ItemVector;
  ItemVector const& items = cli.GetItems();
  for (auto const& item : items) {
    if (item.Target &&
        item.Target->GetType() == cmStateEnums::INTERFACE_LIBRARY) {
      continue;
    }
    if (item.IsPath || item.Target) {
      // case directly handled by the Fastbuild generator
      // (these items will be put in the .Libraries Fastbuild var)
      continue;
    } else {
      linkLibs += item.Value;
    }
    linkLibs += " ";
  }
  return linkLibs;
}
