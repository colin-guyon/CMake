/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#include "cmLocalFastbuildGenerator.h"
#include "cmGeneratorTarget.h"
#include "cmGlobalFastbuildGenerator.h"
#include "cmMakefile.h"
#include "cmSourceFile.h"
#include "cmState.h"
#include "cmSystemTools.h"
#include "cmCustomCommandGenerator.h"
#ifdef _WIN32
#include "windows.h"
#endif
#define FASTBUILD_DOLLAR_TAG "FASTBUILD_DOLLAR_TAG"
//----------------------------------------------------------------------------
cmLocalFastbuildGenerator::cmLocalFastbuildGenerator(cmGlobalFastbuildGenerator* gg, cmMakefile* makefile)
    : cmLocalCommonGenerator(gg, makefile, makefile->GetCurrentBinaryDirectory())
{
	//this->LinkScriptShell = true;
}

//----------------------------------------------------------------------------
cmLocalFastbuildGenerator::~cmLocalFastbuildGenerator()
{

}

//----------------------------------------------------------------------------
void cmLocalFastbuildGenerator::Generate()
{
	// Debug messages
	//std::cout << "======== LOCAL Fastbuild Gen ========\n";
	//GetMakefile()->Print();

	// Now generate information for this generator
}

//----------------------------------------------------------------------------
std::string cmLocalFastbuildGenerator::ConvertToLinkReference(
	std::string const& lib,
    OutputFormat format)
{
	return "";// this->Convert(lib, HOME_OUTPUT, format);
}

//----------------------------------------------------------------------------
void cmLocalFastbuildGenerator::ComputeObjectFilenames(
	std::map<cmSourceFile const*, std::string>& mapping,
	cmGeneratorTarget const* gt)
{
	for (std::map<cmSourceFile const*, std::string>::iterator
		si = mapping.begin(); si != mapping.end(); ++si)
	{
		cmSourceFile const* sf = si->first;
		si->second = this->GetObjectFileNameWithoutTarget(*sf,
			gt->ObjectDirectory);
	}
}

//----------------------------------------------------------------------------
std::string cmLocalFastbuildGenerator::GetTargetDirectory(
	cmGeneratorTarget const* target) const
{
	std::string dir = cmake::GetCMakeFilesDirectoryPostSlash();
	dir += target->GetName();
#if defined(__VMS)
	dir += "_dir";
#else
	dir += ".dir";
#endif
	return dir;
}

//----------------------------------------------------------------------------
void cmLocalFastbuildGenerator::AppendFlagEscape(std::string& flags,
	const std::string& rawFlag) const
{
	std::string escapedFlag = this->EscapeForShell(rawFlag);
	// Other make systems will remove the double $ but
	// fastbuild uses ^$ to escape it. So switch to that.
	//cmSystemTools::ReplaceString(escapedFlag, "$$", "^$");
	this->AppendFlags(flags, escapedFlag);
}

//----------------------------------------------------------------------------
