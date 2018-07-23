/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#ifndef cmGlobalFastbuildGenerator_h
#define cmGlobalFastbuildGenerator_h

#include "cmGlobalCommonGenerator.h"

class cmGlobalGeneratorFactory;
struct cmDocumentationEntry;

/** \class cmGlobalFastbuildGenerator
 * \brief Class for global fastbuild generator.
 */
class cmGlobalFastbuildGenerator 
	: public cmGlobalCommonGenerator
{
public:
	cmGlobalFastbuildGenerator(cmake* cm);
	virtual ~cmGlobalFastbuildGenerator();

	static cmGlobalGeneratorFactory* NewFactory();

	void EnableLanguage(
		std::vector<std::string>const &  lang,
		cmMakefile *mf, bool optional) override;
	void Generate() override;
	void GenerateBuildCommand(
		std::vector<std::string>& makeCommand,
		const std::string& makeProgram,
		const std::string& projectName,
		const std::string& projectDir,
		const std::string& targetName,
		const std::string& config,
		bool fast, int jobs, bool verbose,
		std::vector<std::string> const& makeOptions) override;

	///! create the correct local generator
	cmLocalGenerator *CreateLocalGenerator(cmMakefile* mf) override;
	std::string GetName() const override;

	bool IsMultiConfig() const override { return true; }

	void AppendDirectoryForConfig(
		const std::string& prefix,
		const std::string& config,
		const std::string& suffix,
		std::string& dir) override;

	void ComputeTargetObjectDirectory(cmGeneratorTarget*) const override;
	const char* GetCMakeCFGIntDir() const override;

	std::string ExpandCFGIntDir(
		const std::string& str,
		const std::string& config) const override;

	virtual void GetTargetSets(TargetDependSet& projectTargets,
							   TargetDependSet& originalTargets,
							   cmLocalGenerator* root, GeneratorVector const&);

	const std::vector<std::string> & GetConfigurations() const;
    const std::map<std::string,std::string> & GetVSConfigAlias() const;

    /** Set the generator-specific platform name.  Returns true if platform
    is supported and false otherwise.  */
    virtual bool SetGeneratorPlatform(std::string const& p, cmMakefile* mf);

    virtual void setDefaultPlatform(std::string const& p);

    std::string const& GetPlatformName() const;

    static void GetDocumentation(cmDocumentationEntry& entry);

    static std::string GetActualName();

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

	std::string ConvertToFastbuildPath(const std::string& path);

	template <typename T>
	std::vector<std::string> ConvertToFastbuildPath(const T& container)
	{
		std::vector<std::string> ret;
		for (typename T::const_iterator it = container.begin();
			it != container.end(); ++it) {
		ret.push_back(ConvertToFastbuildPath(*it));
		}
		return ret;
	}

	std::string GetManifestsAsFastbuildPath(
		cmGeneratorTarget& target,
		const std::string& configName);

private:
	class Detail;

	std::vector<std::string> Configurations;
    std::map<std::string, std::string> VSConfigAlias;
    std::string DefaultPlatformName;
    std::string GeneratorPlatform;
};

#endif
