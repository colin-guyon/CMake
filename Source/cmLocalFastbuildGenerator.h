/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#ifndef cmLocalFastbuildGenerator_h
#define cmLocalFastbuildGenerator_h

#include "cmLocalCommonGenerator.h"

class cmSourceFile;
class cmSourceGroup;
class cmCustomCommand;
class cmCustomCommandGenerator;

/** \class cmLocalFastbuildGenerator
 * \brief Base class for Visual Studio generators.
 *
 * cmLocalFastbuildGenerator provides functionality common to all
 * Visual Studio generators.
 */
class cmLocalFastbuildGenerator : public cmLocalCommonGenerator
{
public:
  cmLocalFastbuildGenerator(cmGlobalGenerator* gg, cmMakefile* makefile);
  ~cmLocalFastbuildGenerator() override;

  void Generate() override;

  std::string ConvertToLinkReference(std::string const& lib,
                                     OutputFormat format);
  void ComputeObjectFilenames(
    std::map<cmSourceFile const*, std::string>& mapping,
    cmGeneratorTarget const* gt) override;
  std::string GetTargetDirectory(const cmGeneratorTarget* gt) const
    override;
  void AppendFlagEscape(std::string& flags,
                        const std::string& rawFlag) const override;
};

#endif
