/*============================================================================
  CMake - Cross Platform Makefile Generator
  Copyright 2000-2009 Kitware, Inc., Insight Software Consortium

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/
#ifndef cmGlobalFastbuildGenerator_h
#define cmGlobalFastbuildGenerator_h

#include "cmGlobalCommonGenerator.h"
#  include "cmGlobalGeneratorFactory.h"


/** \class cmGlobalFastbuildGenerator
 * \brief Class for global fastbuild generator.
 */
class cmGlobalFastbuildGenerator 
	: public cmGlobalCommonGenerator
{
public:
	cmGlobalFastbuildGenerator();
	virtual ~cmGlobalFastbuildGenerator();

	static cmGlobalGeneratorFactory* NewFactory() {
        return new cmGlobalGeneratorSimpleFactory<cmGlobalFastbuildGenerator>(); }

	void EnableLanguage(
		std::vector<std::string>const &  lang,
		cmMakefile *mf, bool optional);
	virtual void Generate();
	virtual void GenerateBuildCommand(
		std::vector<std::string>& makeCommand,
		const std::string& makeProgram,
		const std::string& projectName,
		const std::string& projectDir,
		const std::string& targetName,
		const std::string& config,
		bool fast, bool verbose,
		std::vector<std::string> const& makeOptions);

	///! create the correct local generator
	virtual cmLocalGenerator *CreateLocalGenerator();
	virtual std::string GetName() const;

	virtual bool IsMultiConfig() { return true; }

	virtual void AppendDirectoryForConfig(
		const std::string& prefix,
		const std::string& config,
		const std::string& suffix,
		std::string& dir);

	virtual void ComputeTargetObjectDirectory(cmGeneratorTarget*) const;
	virtual const char* GetCMakeCFGIntDir() const;

	virtual void GetTargetSets(TargetDependSet& projectTargets,
							   TargetDependSet& originalTargets,
							   cmLocalGenerator* root, GeneratorVector const&);

	const std::vector<std::string> & GetConfigurations() const;

  /**
   * Utilized by the generator factory to determine if this generator
   * supports toolsets.
   */
  static bool SupportsToolset() { return false; }

  /**
   * Utilized by the generator factory to determine if this generator
   * supports platforms.
   */
  static bool SupportsPlatform() { return false; }

  static std::string GetActualName() { return "Fastbuild"; }

  static void GetDocumentation(cmDocumentationEntry& entry);
private:
	class Factory;
	class Detail;

	std::vector<std::string> Configurations;
};

#endif
