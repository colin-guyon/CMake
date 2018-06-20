/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#include "cmFastbuildTargetGenerator.h"

#include "cmLocalCommonGenerator.h"
#include "cmGlobalGenerator.h"
#include "cmGeneratorTarget.h"
#include "cmGlobalFastbuildGenerator.h"

cmFastbuildTargetGenerator::cmFastbuildTargetGenerator(cmGeneratorTarget* gt)
  : cmCommonTargetGenerator(gt)
{
}

void cmFastbuildTargetGenerator::AddIncludeFlags(std::string& /* flags */,
                                                 const std::string& /* lang */)
{
}

std::string cmFastbuildTargetGenerator::ConvertToFastbuildPath(
  const std::string& path)
{
  return ((cmGlobalFastbuildGenerator*)GeneratorTarget->GetGlobalGenerator())
    ->ConvertToFastbuildPath(path);
}
