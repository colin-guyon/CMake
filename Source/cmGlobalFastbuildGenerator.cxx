/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */

/*============================================================================
    Development progress:

    Tasks/Issues:
     - Linker for msvc uses the cmake command to callback. Don't think this is
an issue since
     I think compilation is the part that gets distributed anyway.
     But it might mean that the cache has trouble calculating deps for
obj->lib/exe.
     Not sure if Fastbuild supports that anyway yet
     - Need to sort custom build commands by their outputs

    Fastbuild bugs:
     - Defining prebuild dependencies that don't exist, causes the error output
when that
     target is actually defined. Rather than originally complaining that the
target
     doesn't exist where the reference is attempted.
     - Parsing strings with double $$ doesn't generate a nice error
     - Undocumented that you can escape a $ with ^$
     - ExecInputs is invalid empty
     - Would be great if you could define dummy targets (maybe blank aliases?)
     - Exec nodes need to not worry about dummy output files not being created
     - Would be nice if nodes didn't need to be completely in order. But then
cycles would be possible
     - Implib directory is not created for exeNodes (DLLs work now though)

    Limitations:
     - Only tested/working with MSVC

    Notes:
     - Understanding Custom Build Steps and Build Events
         https://msdn.microsoft.com/en-us/library/e85wte0k.aspx
     very useful documentation detailing the order of execution
     of standard MSVC events. This is useful to determine correct
     behaviour of fastbuild generator (view a project generated to MSVC,
     then apply the same rules/assumptions back into fastbuild).
     i.e. Custom rules are always executed first.

    Current list of unit tests failing:
================ 3.2 ===========================
    91% tests passed, 32 tests failed out of 371
    Total Test time (real) = 1479.86 sec

The following tests FAILED:
59 - Preprocess (Failed)
60 - ExportImport (Failed)
77 - Module.ExternalData (Failed)
108 - CustomCommandByproducts (Failed)
112 - BuildDepends (Failed)
113 - SimpleInstall (Failed)
114 - SimpleInstall-Stage2 (Failed)
131 - ExternalProjectLocal (Failed)
132 - ExternalProjectUpdateSetup (Failed)
133 - ExternalProjectUpdate (Failed)
274 - RunCMake.Configure (Failed)
370 - CMake.CheckSourceTree (Failed)

================ 3.3 ===========================
97% tests passed, 11 tests failed out of 381

Regressions:
274 - RunCMake.CMP0060 (Failed)
================ 3.4 ===========================
95% tests passed, 19 tests failed out of 397

Regressions:
280 - RunCMake.BuildDepends (Failed) -- add dependency on the manifest file
this is the Ninja code that does it
      std::vector<cmSourceFile const*> manifest_srcs;
      this->GeneratorTarget->GetManifests(manifest_srcs, configName);
      for (std::vector<cmSourceFile const*>::iterator mi = manifest_srcs.begin();
           mi != manifest_srcs.end(); ++mi) {
        command.sourceFiles[srcFile->GetLocation().GetDirectory()].push_back(
          sourceFile);
      }
366 - RunCMake.AutoExportDll (Failed) -- very specific, auto-generation of .def
      file

================ 3.5 ===========================
94% tests passed, 23 tests failed out of 398

Regressions:
109 - CustomCommand (Failed)
153 - Plugin (SEGFAULT)
277 - RunCMake.CMP0065 (Failed)
debug with the following command:
G:\cmake\cmake_fbuild_3.6\bin\Debug\cmake.exe "-DCMAKE_MODULE_PATH=G:/cmake/CMake/Tests/RunCMake" "-DRunCMake_GENERATOR=Fastbuild" "-DRunCMake_GENERATOR_PLATFORM=" "-DRunCMake_GENERATOR_TOOLSET=" "-DRunCMak
e_MAKE_PROGRAM=G:/tools/FBuild.exe" "-DRunCMake_SOURCE_DIR=G:/cmake/CMake/Tests/RunCMake/CMP0065" "-DRunCMake_BINARY_DIR=G:/cmake/cmake_fbuild_3.6/Tests/RunCMake/CMP0065" "-P" "G:/cmake/CMake/Tests/RunCMake/CMP0065/RunCMakeTest.cmake"

============================================================================*/
#include "cmGlobalFastbuildGenerator.h"

#include "cmComputeLinkInformation.h"
#include "cmCustomCommandGenerator.h"
#include "cmGeneratorTarget.h"
#include "cmDocumentationEntry.h"
#include "cmGlobalGeneratorFactory.h"
#include "cmGlobalVisualStudioGenerator.h"
#include "cmLocalFastbuildGenerator.h"
#include "cmLocalGenerator.h"
#include "cmMakefile.h"
#include "cmSourceFile.h"
#include "cmTarget.h"
#include "cmState.h"
#include <assert.h>
#include <cmsys/Encoding.hxx>

#include "cmFastbuildNormalTargetGenerator.h"
#include "cmFastbuildUtilityTargetGenerator.h"

const char* cmGlobalFastbuildGenerator::FASTBUILD_DOLLAR_TAG =
  "FASTBUILD_DOLLAR_TAG";
#define FASTBUILD_DOLLAR_TAG "FASTBUILD_DOLLAR_TAG"

cmGlobalFastbuildGenerator::Detail::FileContext::FileContext()
{
}

void cmGlobalFastbuildGenerator::Detail::FileContext::setFileName(
  const std::string fileName)
{
  fout.Open(fileName.c_str());
}

void cmGlobalFastbuildGenerator::Detail::FileContext::close()
{
  // Close file
  fout.Close();
}

void cmGlobalFastbuildGenerator::Detail::FileContext::WriteComment(
  const std::string& comment)
{
  fout << linePrefix << ";" << comment << "\n";
}

void cmGlobalFastbuildGenerator::Detail::FileContext::WriteBlankLine()
{
  fout << "\n";
}

void cmGlobalFastbuildGenerator::Detail::FileContext::WriteHorizontalLine()
{
  fout << ";----------------------------------------------------------------"
          "---------------\n";
}

void cmGlobalFastbuildGenerator::Detail::FileContext::WriteSectionHeader(
  const char* section)
{
  fout << "\n";
  WriteHorizontalLine();
  WriteComment(section);
  WriteHorizontalLine();
}

void cmGlobalFastbuildGenerator::Detail::FileContext::WritePushScope(
  char begin, char end)
{
  fout << linePrefix << begin << "\n";
  linePrefix += "\t";
  closingScope += end;
}

void cmGlobalFastbuildGenerator::Detail::FileContext::WritePushScopeStruct()
{
  WritePushScope('[', ']');
}

void cmGlobalFastbuildGenerator::Detail::FileContext::WritePopScope()
{
  assert(!linePrefix.empty());
  linePrefix.resize(linePrefix.size() - 1);

  fout << linePrefix << closingScope[closingScope.size() - 1] << "\n";

  closingScope.resize(closingScope.size() - 1);
}

void cmGlobalFastbuildGenerator::Detail::FileContext::WriteVariableDeclare(
  const std::string& key,
  const std::string& operation,
  bool newLine)
{
  fout << linePrefix << "." << key << " " << operation << (newLine?" \n":" ") ;
}

void cmGlobalFastbuildGenerator::Detail::FileContext::WriteVariableAssign(
  const std::string& key,
  const std::string& value)
{
  WriteVariableDeclare(key,"=",false);
  fout  << "." <<value << "\n";
}

void cmGlobalFastbuildGenerator::Detail::FileContext::WriteVariableBool(
  const std::string& key,
  bool value)
{
  WriteVariableDeclare(key,"=",false);
  fout  << (value?"true":"false") << "\n";
}

void cmGlobalFastbuildGenerator::Detail::FileContext::WriteVariable(
  const std::string& key,
  const std::string& value,
  const std::string& operation)
{
  WriteVariableDeclare(key,operation,false);
  fout  << Quote(value) << "\n";
}

void cmGlobalFastbuildGenerator::Detail::FileContext::WriteCommand(
  const std::string& command, const std::string& value)
{
  fout << linePrefix << command;
  if (!value.empty()) {
    fout << "(" << value << ")";
  }
  fout << "\n";
}

void cmGlobalFastbuildGenerator::Detail::FileContext::WriteArray(
  const std::string& key, const std::vector<std::string>& values,
  const std::string& operation)
{
  WriteVariableDeclare(key,operation);
  WritePushScope();
  size_t size = values.size();
  for (size_t index = 0; index < size; ++index) {
    const std::string& value = values[index];
    bool isLast = index == size - 1;

    fout << linePrefix << value;
    if (!isLast) {
      fout << ',';
    }
    fout << "\n";
  }
  WritePopScope();
}

//----------------------------------------------------------------------------
void cmGlobalFastbuildGenerator::Detail::Detection::DetectLanguages(
  std::set<std::string>& languages, const cmGeneratorTarget* generatorTarget)
{
  // Object libraries do not have linker stages
  // nor utilities
  bool hasObjectGroups = generatorTarget->GetType() != cmStateEnums::UTILITY &&
    generatorTarget->GetType() != cmStateEnums::GLOBAL_TARGET;
  if (!hasObjectGroups) {
    return;
  }

  std::vector<std::string> configs;
  generatorTarget->Makefile->GetConfigurations(configs, false);
  for (std::vector<std::string>::const_iterator iter = configs.begin();
       iter != configs.end(); ++iter) {
    const std::string& configName = *iter;

    std::vector<const cmSourceFile*> sourceFiles;
    generatorTarget->GetObjectSources(sourceFiles, configName);
    for (std::vector<const cmSourceFile*>::const_iterator i =
           sourceFiles.begin();
         i != sourceFiles.end(); ++i) {
      const std::string& lang = (*i)->GetLanguage();
      if (!lang.empty()) {
        languages.insert(lang);
      }
    }
  }
}

void cmGlobalFastbuildGenerator::Detail::Detection::FilterSourceFiles(
  std::vector<cmSourceFile const*>& filteredSourceFiles,
  std::vector<cmSourceFile const*>& sourceFiles, const std::string& language)
{
  for (std::vector<cmSourceFile const*>::const_iterator i =
         sourceFiles.begin();
       i != sourceFiles.end(); ++i) {
    const cmSourceFile* sf = *i;
    if (sf->GetLanguage() == language) {
      filteredSourceFiles.push_back(sf);
    }
  }
}

void cmGlobalFastbuildGenerator::Detail::Detection::DetectCompilerFlags(
  std::string& compileFlags, cmLocalCommonGenerator* lg,
  const cmGeneratorTarget* gt, const cmSourceFile* source,
  const std::string& language, const std::string& configName)
{
  lg->AddLanguageFlags(compileFlags, gt, language, configName);

  lg->AddArchitectureFlags(compileFlags, gt, language, configName);

  // Add shared-library flags if needed.
  lg->AddCMP0018Flags(compileFlags, gt, language, configName);

  lg->AddVisibilityPresetFlags(compileFlags, gt, language);

  std::vector<std::string> includes;
  lg->GetIncludeDirectories(includes, gt, language, configName);

  // Add include directory flags.
  // FIXME const cast are evil
  std::string includeFlags = lg->GetIncludeFlags(
    includes, (cmGeneratorTarget*)gt, language,
    language == "RC" ? true : false, // full include paths for RC
    // needed by cmcldeps
    false, configName);

  lg->AppendFlags(compileFlags, includeFlags);

  // Append old-style preprocessor definition flags.
  lg->AppendFlags(compileFlags, lg->GetMakefile()->GetDefineFlags());

  // Add target-specific flags.
  // FIXME const cast are evil
  lg->AddCompileOptions(compileFlags, (cmGeneratorTarget*)gt, language,
                        configName);

  if (source) {
    lg->AppendFlags(compileFlags, source->GetProperty("COMPILE_FLAGS"));
  }
}

void cmGlobalFastbuildGenerator::Detail::Detection::DependencySorter::
  TargetHelper::GetOutputs(const cmGeneratorTarget* entry,
                           std::vector<std::string>& outputs)
{
  outputs.push_back(entry->GetName());
}

void cmGlobalFastbuildGenerator::Detail::Detection::DependencySorter::
  TargetHelper::GetInputs(const cmGeneratorTarget* entry,
                          std::vector<std::string>& inputs)
{
  TargetDependSet const& ts = gg->GetTargetDirectDepends(entry);
  for (TargetDependSet::const_iterator iter = ts.begin(); iter != ts.end();
       ++iter) {
    const cmGeneratorTarget* dtarget = *iter;
    inputs.push_back(dtarget->GetName());
  }
}

void cmGlobalFastbuildGenerator::Detail::Detection::DependencySorter::
  CustomCommandHelper::GetOutputs(const cmSourceFile* entry,
                                  std::vector<std::string>& outputs)
{
  const cmCustomCommand* cc = entry->GetCustomCommand();

  // We need to generate the command for execution.
  cmCustomCommandGenerator ccg(*cc, configName, lg);

  const std::vector<std::string>& ccOutputs = ccg.GetOutputs();
  const std::vector<std::string>& byproducts = ccg.GetByproducts();
  outputs.insert(outputs.end(), ccOutputs.begin(), ccOutputs.end());
  outputs.insert(outputs.end(), byproducts.begin(), byproducts.end());
}

void cmGlobalFastbuildGenerator::Detail::Detection::DependencySorter::
  CustomCommandHelper::GetInputs(const cmSourceFile* entry,
                                 std::vector<std::string>& inputs)
{
  const cmCustomCommand* cc = entry->GetCustomCommand();
  cmMakefile* makefile = lg->GetMakefile();
  cmCustomCommandGenerator ccg(*cc, configName, lg);

  std::string workingDirectory = ccg.GetWorkingDirectory();
  if (workingDirectory.empty()) {
    workingDirectory = makefile->GetCurrentBinaryDirectory();
  }
  if (workingDirectory.back() != '/') {
    workingDirectory += "/";
  }

  // Take the dependencies listed and split into targets and files.
  const std::vector<std::string>& depends = ccg.GetDepends();
  for (std::vector<std::string>::const_iterator iter = depends.begin();
       iter != depends.end(); ++iter) {
    std::string dep = *iter;

    bool isTarget = gg->FindTarget(dep) != NULL;
    if (!isTarget) {
      if (!cmSystemTools::FileIsFullPath(dep.c_str())) {
        dep = workingDirectory + dep;
      }

      inputs.push_back(dep);
    }
  }
}

void cmGlobalFastbuildGenerator::ComputeTargetOrderAndDependencies(
  Detail::Detection::OrderedTargetSet& orderedTargets)
{
  TargetDependSet projectTargets;
  TargetDependSet originalTargets;
  std::map<std::string, std::vector<cmLocalGenerator *> >::const_iterator
    it = this->GetProjectMap().begin(),
    end = this->GetProjectMap().end();
  for (; it != end; ++it) {
    const std::vector<cmLocalGenerator*>& generators = it->second;
    cmLocalFastbuildGenerator* root =
      static_cast<cmLocalFastbuildGenerator*>(generators[0]);

    // Given this information, calculate the dependencies:
    // Collect all targets under this root generator and the transitive
    // closure of their dependencies.

    this->GetTargetSets(projectTargets, originalTargets, root, generators);
  }

  // Iterate over the targets and export their order
  for (TargetDependSet::iterator iter = projectTargets.begin();
       iter != projectTargets.end(); ++iter) {
    const cmTargetDepend& targetDepend = *iter;
    const cmGeneratorTarget* target = targetDepend.operator->();

    orderedTargets.push_back(target);
  }

  Detail::Detection::DependencySorter::TargetHelper targetHelper = { this };
  Detail::Detection::DependencySorter::Sort(targetHelper, orderedTargets);
  Detail::Detection::StripNestedGlobalTargets(orderedTargets);
}

// Iterate over all targets and remove the ones that are
// not needed for generation.
// i.e. the nested global targets
bool cmGlobalFastbuildGenerator::Detail::Detection::RemovalTest::operator()(
  const cmGeneratorTarget* target) const
{
  if (target->GetType() == cmStateEnums::GLOBAL_TARGET) {
    // We only want to process global targets that live in the home
    // (i.e. top-level) directory.  CMake creates copies of these targets
    // in every directory, which we don't need.
    cmMakefile* mf = target->Target->GetMakefile();
    if (strcmp(mf->GetCurrentSourceDirectory(), mf->GetHomeDirectory().c_str()) != 0) {
      return true;
    }
  }
  return false;
}

void cmGlobalFastbuildGenerator::Detail::Detection::StripNestedGlobalTargets(
  OrderedTargetSet& orderedTargets)
{
  orderedTargets.erase(std::remove_if(orderedTargets.begin(),
                                      orderedTargets.end(), RemovalTest()),
                       orderedTargets.end());
}

void cmGlobalFastbuildGenerator::Detail::Detection::DetectCompilerExtraFiles(
  const std::string& compilerID, const std::string& version,
  std::vector<std::string>& extraFiles)
{
  // Output a list of files that are relative to $CompilerRoot$
  return;

  if (compilerID == "MSVC") {
    if (static_cast<size_t>(version.compare(0, 3, "18.")) != std::string::npos) {
      // Using vs2013
      const char* vs2013_extraFiles[13] = {
        "$CompilerRoot$\\c1.dll", "$CompilerRoot$\\c1ast.dll",
        "$CompilerRoot$\\c1xx.dll", "$CompilerRoot$\\c1xxast.dll",
        "$CompilerRoot$\\c2.dll", "$CompilerRoot$\\msobj120.dll",
        "$CompilerRoot$\\mspdb120.dll", "$CompilerRoot$\\mspdbcore.dll",
        "$CompilerRoot$\\mspft120.dll", "$CompilerRoot$\\1033\\clui.dll",
        "$CompilerRoot$\\..\\..\\redist\\x86\\Microsoft.VC120.CRT\\msvcp120."
        "dll",
        "$CompilerRoot$\\..\\..\\redist\\x86\\Microsoft.VC120.CRT\\msvcr120."
        "dll",
        "$CompilerRoot$\\..\\..\\redist\\x86\\Microsoft.VC120."
        "CRT\\vccorlib120.dll"
      };
      extraFiles.insert(extraFiles.end(), &vs2013_extraFiles[0],
                        &vs2013_extraFiles[13]);
    } else if (static_cast<size_t>(version.compare(0, 3, "17.")) != std::string::npos) {
      // Using vs2012
      const char* vs2012_extraFiles[12] = {
        "$CompilerRoot$\\c1.dll", "$CompilerRoot$\\c1ast.dll",
        "$CompilerRoot$\\c1xx.dll", "$CompilerRoot$\\c1xxast.dll",
        "$CompilerRoot$\\c2.dll", "$CompilerRoot$\\mspft110.dll",
        "$CompilerRoot$\\1033\\clui.dll", "$CompilerRoot$\\..\\.."
                                          "\\redist\\x86\\Microsoft.VC110."
                                          "CRT\\msvcp110.dll",
        "$CompilerRoot$\\..\\..\\redist\\x86\\Microsoft.VC110.CRT\\msvcr110."
        "dll",
        "$CompilerRoot$\\..\\..\\redist\\x86\\Microsoft.VC110."
        "CRT\\vccorlib110.dll",
        "$CompilerRoot$\\..\\..\\..\\Common7\\IDE\\mspdb110.dll",
        "$CompilerRoot$\\..\\..\\..\\Common7\\IDE\\mspdbcore.dll"
      };
      extraFiles.insert(extraFiles.end(), &vs2012_extraFiles[0],
                        &vs2012_extraFiles[12]);
    }
  }
}

//----------------------------------------------------------------------------

std::string cmGlobalFastbuildGenerator::Quote(const std::string& str,
                                              const std::string& quotation)
{
  std::string result = str;
  cmSystemTools::ReplaceString(result, "^", "^^");
  cmSystemTools::ReplaceString(result, quotation, "^" + quotation);
  cmSystemTools::ReplaceString(result, "$", "^$" );
  cmSystemTools::ReplaceString(result, FASTBUILD_DOLLAR_TAG, "$");
  return quotation + result + quotation;
}

std::string cmGlobalFastbuildGenerator::Detail::Generation::Join(
  const std::vector<std::string>& elems, const std::string& delim)
{
  std::stringstream stringstream;
  for (std::vector<std::string>::const_iterator iter = elems.begin();
       iter != elems.end(); ++iter) {
    stringstream << (*iter);
    if (iter + 1 != elems.end()) {
      stringstream << delim;
    }
  }

  return stringstream.str();
}

std::vector<std::string> cmGlobalFastbuildGenerator::Wrap(
  const std::vector<std::string>& in, const std::string& prefix,
  const std::string& suffix)
{
  std::vector<std::string> result;

  cmGlobalFastbuildGenerator::Detail::Generation::WrapHelper helper = {
    prefix, suffix
  };

  std::transform(in.begin(), in.end(), std::back_inserter(result), helper);

  return result;
}

std::string cmGlobalFastbuildGenerator::Detail::Generation::EncodeLiteral(
  const std::string& lit)
{
  std::string result = lit;
  cmSystemTools::ReplaceString(result, "$", "^$");
  return result;
}

void cmGlobalFastbuildGenerator::Detail::Generation::BuildTargetContexts(
  cmGlobalFastbuildGenerator* gg, TargetContextList& map)
{
  std::map<std::string, std::vector<cmLocalGenerator *> >::const_iterator
    it = gg->GetProjectMap().begin(),
    end = gg->GetProjectMap().end();
  for (; it != end; ++it) {
    const std::vector<cmLocalGenerator*>& generators = it->second;
    cmLocalFastbuildGenerator* root =
      static_cast<cmLocalFastbuildGenerator*>(generators[0]);

    // Build a map of all targets to their local generator
    for (std::vector<cmLocalGenerator*>::const_iterator iter =
           generators.begin();
         iter != generators.end(); ++iter) {
      cmLocalGenerator* lg = *iter;

      if (gg->IsExcluded(root, lg)) {
        continue;
      }

      for (std::vector<cmGeneratorTarget*>::const_iterator targetIter =
             lg->GetGeneratorTargets().begin();
           targetIter != lg->GetGeneratorTargets().end(); ++targetIter) {
        cmGeneratorTarget* generatorTarget = *targetIter;
        if (gg->IsRootOnlyTarget(generatorTarget) &&
            lg->GetMakefile() != root->GetMakefile()) {
          continue;
        }
        map.insert(generatorTarget);
      }
    }
  }
}

static std::string GenBffPath(const std::string& rootDir,
                              const std::string& name)
{
  return rootDir + "/" + name + ".bff";
}

void cmGlobalFastbuildGenerator::Detail::BFFFiles::Init(
  const std::string& root, const std::vector<std::string>& configs)
{
  rootDir = root;
  main.setFileName(GenBffPath(rootDir, "fbuild"));
  base.setFileName(GenBffPath(rootDir, "base"));

  for (std::vector<std::string>::const_iterator iter = configs.cbegin();
       iter != configs.cend(); ++iter) {
    std::string configName = *iter;

    FileContext& ret = perConfig[configName];
    ret.setFileName(GenBffPath(rootDir, configName));
    ret.WriteSectionHeader(("Fastbuild config for :" + configName).c_str());
    ret.WriteDirective("include \"base.bff\"");
  }
}

void cmGlobalFastbuildGenerator::Detail::BFFFiles::Close(cmGlobalGenerator* gg)
{
  main.close();
  base.close();
  gg->FileReplacedDuringGenerate(GenBffPath(rootDir, "fbuild"));
  gg->FileReplacedDuringGenerate(GenBffPath(rootDir, "base"));
  for (std::map<std::string, FileContext>::iterator it = perConfig.begin();
       it != perConfig.end(); ++it) {
    it->second.close();
    gg->FileReplacedDuringGenerate(GenBffPath(rootDir, it->first));
  }
}

void cmGlobalFastbuildGenerator::Detail::Generation::DetectDuplicate(
  GenerationContext& context, std::vector<std::string>& configs)
{

  std::ostringstream w;
  w << "duplicate outputs in different config :\n";

  DuplicateOutputs& duplicateOutputs =
    ((cmGlobalFastbuildGenerator*)context.root->GetGlobalGenerator())
      ->g_duplicateOutputs;

  bool hasDuplicate = false;
  // checking if there is duplicate output in different config
  for (DuplicateOutputs::const_iterator iter = duplicateOutputs.cbegin();
       iter != duplicateOutputs.cend(); ++iter) {
    if (iter->second.size() > 1) {
      hasDuplicate = true;
      w << iter->first << " in configs:";
      std::copy(iter->second.begin(), iter->second.end(),
                std::ostream_iterator<std::string>(w, ";"));
      w << "\n";
    }
  }
  if (!hasDuplicate) {
    return;
  }

  configs.resize(1);
  w << "fastbuild generator fallback to build " << configs.front()
    << " config only\n";

  context.root->GetCMakeInstance()->IssueMessage(cmake::WARNING, w.str());
}

void cmGlobalFastbuildGenerator::Detail::Generation::GenerateRootBFF(
  cmGlobalFastbuildGenerator* self)
{

  cmLocalFastbuildGenerator* root =
    static_cast<cmLocalFastbuildGenerator*>(self->GetLocalGenerators()[0]);

  std::string rootDir = root->GetMakefile()->GetHomeOutputDirectory();

  std::vector<std::string> configs;
  root->GetMakefile()->GetConfigurations(configs, false);
  self->g_bffFiles.Init(rootDir, configs);

  GenerationContext context(root, self->g_bffFiles);
  self->ComputeTargetOrderAndDependencies(context.orderedTargets);
  BuildTargetContexts(self, context.targetContexts);
  // write root bff
  self->g_bffFiles.base.WriteSectionHeader(
    "Fastbuild makefile - Generated using CMAKE");
  self->g_bffFiles.base.WriteDirective("once");

  WritePlaceholders(self->g_bffFiles.base);
  WriteSettings(self->g_bffFiles.base,
                self->GetCMakeInstance()->GetHomeOutputDirectory());
  WriteCompilers(context);
  WriteConfigurations(self->g_bffFiles, root->GetMakefile());

  // Sort targets
  WriteTargetDefinitions(context, false);

  DetectDuplicate(context, configs);
  for (std::vector<std::string>::const_iterator iter = configs.cbegin();
       iter != configs.cend(); ++iter) {
    self->g_bffFiles.main.WriteDirective("include \"" + *iter + ".bff\"");
  }

  // Write out a list of all configs
  self->g_bffFiles.main.WriteArray("all_configs",
                                   Wrap(configs, ".config_", ""));

  WriteAliases(context, self, false, configs);
  WriteTargetDefinitions(context, true);
  WriteAliases(context, self, true, configs);

  WriteBFFRebuildTarget(self,self->g_bffFiles.main);
  self->g_bffFiles.Close(self);
}

void cmGlobalFastbuildGenerator::Detail::Generation::WritePlaceholders(
  FileContext& fileContext)
{
  // Define some placeholder
  fileContext.WriteSectionHeader("Helper variables");

  fileContext.WriteVariable("FB_INPUT_1_PLACEHOLDER", "\"%1\"");
  fileContext.WriteVariable("FB_INPUT_2_PLACEHOLDER", "\"%2\"");
}

void cmGlobalFastbuildGenerator::Detail::Generation::WriteSettings(
  FileContext& fileContext, std::string cacheDir)
{
  fileContext.WriteSectionHeader("Settings");

  fileContext.WriteCommand("Settings");
  fileContext.WritePushScope();

  cacheDir += "\\.fbuild.cache";
  cmSystemTools::ConvertToOutputSlashes(cacheDir);

  fileContext.WriteVariable("CachePath", cacheDir);
  fileContext.WritePopScope();
}

void cmGlobalFastbuildGenerator::Detail::FileContext::WriteDirective(
  const std::string& directive)
{
  fout << "#" << directive << "\n";
}

bool cmGlobalFastbuildGenerator::Detail::Generation::WriteCompilers(
  GenerationContext& context)
{
  cmMakefile* mf = context.root->GetMakefile();

  FileContext& base = context.bffFiles.base;

  base.WriteSectionHeader("Compilers");

  // Detect each language used in the definitions
  std::set<std::string> languages;
  for (TargetContextList::iterator iter = context.targetContexts.begin();
       iter != context.targetContexts.end(); ++iter) {

    if ((*iter)->GetType() == cmStateEnums::INTERFACE_LIBRARY) {
      continue;
    }

    Detection::DetectLanguages(languages, *iter);
  }

  // Now output a compiler for each of these languages
  typedef std::map<std::string, std::string> StringMap;
  typedef std::map<std::string, CompilerDef> CompilerDefMap;
  CompilerDefMap compilerToDef;
  StringMap languageToCompiler;
  for (std::set<std::string>::iterator iter = languages.begin();
       iter != languages.end(); ++iter) {
    const std::string& language = *iter;

    // Calculate the root location of the compiler
    std::string variableString = "CMAKE_" + language + "_COMPILER";
    std::string compilerLocation = mf->GetSafeDefinition(variableString);
    if (compilerLocation.empty()) {
      return false;
    }

    // Add the language to the compiler's name
    CompilerDef& compilerDef = compilerToDef[compilerLocation];
    if (compilerDef.name.empty()) {
      compilerDef.name = "Compiler";
      compilerDef.path = compilerLocation;
      compilerDef.cmakeCompilerID =
        mf->GetSafeDefinition("CMAKE_" + language + "_COMPILER_ID");
      compilerDef.cmakeCompilerVersion =
        mf->GetSafeDefinition("CMAKE_" + language + "_COMPILER_VERSION");
    }
    compilerDef.name += "-";
    compilerDef.name += language;

    // Now add the language to point to that compiler location
    languageToCompiler[language] = compilerLocation;
  }

  // Now output all the compilers
  for (CompilerDefMap::iterator iter = compilerToDef.begin();
       iter != compilerToDef.end(); ++iter) {
    const CompilerDef& compilerDef = iter->second;

    // Detect the list of extra files used by this compiler
    // for distribution
    std::vector<std::string> extraFiles;
    Detection::DetectCompilerExtraFiles(compilerDef.cmakeCompilerID,
                                        compilerDef.cmakeCompilerVersion,
                                        extraFiles);

    // Strip out the path to the compiler
    std::string compilerPath =
      cmSystemTools::GetFilenamePath(compilerDef.path);
    std::string compilerFile =
      FASTBUILD_DOLLAR_TAG "CompilerRoot" FASTBUILD_DOLLAR_TAG ""

#if defined(_WIN32) 
      "\\"
#else
      "/"
#endif
      + cmSystemTools::GetFilenameName(compilerDef.path);

    cmSystemTools::ConvertToOutputSlashes(compilerPath);
    cmSystemTools::ConvertToOutputSlashes(compilerFile);

    // Write out the compiler that has been configured
    base.WriteCommand("Compiler", Quote(compilerDef.name));
    base.WritePushScope();
    base.WriteVariable("CompilerRoot", compilerPath);
    base.WriteVariable("Executable", compilerFile);
    base.WriteArray("ExtraFiles", Wrap(extraFiles));
    base.WritePopScope();
  }

  // Now output the compiler names according to language as variables
  for (StringMap::iterator iter = languageToCompiler.begin();
       iter != languageToCompiler.end(); ++iter) {
    const std::string& language = iter->first;
    const std::string& compilerLocation = iter->second;
    const CompilerDef& compilerDef = compilerToDef[compilerLocation];

    // Output a default compiler to absorb the library requirements for a
    // compiler
    if (iter == languageToCompiler.begin()) {
      base.WriteVariable("Compiler_dummy", compilerDef.name);
    }

    base.WriteVariable("Compiler_" + language, compilerDef.name);
  }

  return true;
}

void cmGlobalFastbuildGenerator::Detail::Generation::WriteConfigurations(
  BFFFiles& bffFiles, cmMakefile* makefile)
{
  FileContext& base = bffFiles.base;
  base.WriteSectionHeader("Configurations");

  base.WriteVariableDeclare("ConfigBase");
  base.WritePushScopeStruct();
  base.WritePopScope();

  // Iterate over all configurations and define them:
  std::vector<std::string> configs;
  makefile->GetConfigurations(configs, false);
  for (std::vector<std::string>::const_iterator iter = configs.begin();
       iter != configs.end(); ++iter) {
    const std::string& configName = *iter;
    FileContext& perConfig = bffFiles.perConfig[configName];
    perConfig.WriteVariableDeclare("config_" + configName);
    perConfig.WritePushScopeStruct();

    // Using base config
    perConfig.WriteCommand("Using", ".ConfigBase");

    perConfig.WritePopScope();

  }

}

void cmGlobalFastbuildGenerator::Detail::Generation::WriteTargetDefinitions(
  GenerationContext& context, bool outputGlobals)
{
  if (outputGlobals) {
    context.bffFiles.main.WriteSectionHeader("Global Target Definitions");
  }

  // Now iterate each target in order
  for (OrderedTargets::iterator targetIter = context.orderedTargets.begin();
       targetIter != context.orderedTargets.end(); ++targetIter) {
    const cmGeneratorTarget* constTarget = (*targetIter);
    if (constTarget->GetType() == cmStateEnums::INTERFACE_LIBRARY) {
      continue;
    }
    // FIXME const cast are evil
    cmGeneratorTarget* target = (cmGeneratorTarget*)constTarget;

    if (target->GetType() == cmStateEnums::GLOBAL_TARGET) {
      if (!outputGlobals)
        continue;
    } else {
      if (outputGlobals)
        continue;
    }

    switch (target->GetType()) {
      case cmStateEnums::EXECUTABLE:
      case cmStateEnums::SHARED_LIBRARY:
      case cmStateEnums::STATIC_LIBRARY:
      case cmStateEnums::MODULE_LIBRARY:
      case cmStateEnums::OBJECT_LIBRARY:
      // TODO should utility target be treated differently ?
      case cmStateEnums::UTILITY:
      case cmStateEnums::GLOBAL_TARGET: {
        cmFastbuildNormalTargetGenerator targetGenerator(target);
        targetGenerator.Generate();
        break;
      }

      break;
      default:
        break;
    }
  }
}

void cmGlobalFastbuildGenerator::Detail::Generation::WriteAliases(
  GenerationContext& context, cmGlobalFastbuildGenerator* gg,
  bool outputGlobals, const std::vector<std::string>& usedConfigs)
{
  context.bffFiles.main.WriteSectionHeader("Aliases");

  // Write the following aliases:
  // Per Target
  // Per Config
  // All

  typedef std::map<std::string, std::vector<std::string> > TargetListMap;
  TargetListMap perConfig;
  TargetListMap perTarget;

  for (OrderedTargets::iterator targetIter = context.orderedTargets.begin();
       targetIter != context.orderedTargets.end(); ++targetIter) {
    const cmGeneratorTarget* constTarget = (*targetIter);
    if (constTarget->GetType() == cmStateEnums::INTERFACE_LIBRARY) {
      continue;
    }

    // FIXME const cast are evil
    TargetContextList::iterator findResult =
      context.targetContexts.find((cmGeneratorTarget*)constTarget);
    if (findResult == context.targetContexts.end()) {
      continue;
    }

    cmGeneratorTarget* target = *findResult;
    const std::string& targetName = target->GetName();

    if (target->GetType() == cmStateEnums::GLOBAL_TARGET) {
      if (!outputGlobals)
        continue;
    } else {
      if (outputGlobals)
        continue;
    }

    for (std::vector<std::string>::const_iterator iter = usedConfigs.begin();
         iter != usedConfigs.end(); ++iter) {
      const std::string& configName = *iter;
      std::string aliasName = targetName + "-" + configName;

      perTarget[targetName].push_back(aliasName);
      if (!gg->IsExcluded(context.root, target)) {
        perConfig[configName].push_back(aliasName);
      }
    }
  }

  if (!outputGlobals) {
    context.bffFiles.main.WriteComment("Per config");
    std::vector<std::string> aliasTargets;
    for (TargetListMap::iterator iter = perConfig.begin();
         iter != perConfig.end(); ++iter) {
      const std::string& configName = iter->first;
      const std::vector<std::string>& targets = iter->second;

      FileContext& fc = context.bffFiles.perConfig[configName];

      fc.WriteCommand("Alias", Quote(configName));
      fc.WritePushScope();
      fc.WriteArray("Targets", Wrap(targets, "'", "'"));
      fc.WritePopScope();

      aliasTargets.clear();
      aliasTargets.push_back(configName);
      fc.WriteCommand("Alias", Quote("ALL_BUILD-" + configName));
      fc.WritePushScope();
      fc.WriteArray("Targets", Wrap(aliasTargets, "'", "'"));
      fc.WritePopScope();
    }
  }

  context.bffFiles.main.WriteComment("Per targets");
  for (TargetListMap::iterator iter = perTarget.begin();
       iter != perTarget.end(); ++iter) {
    const std::string& targetName = iter->first;
    const std::vector<std::string>& targets = iter->second;

    FileContext& fc = context.bffFiles.main;
    fc.WriteCommand("Alias", "'" + targetName + "'");
    fc.WritePushScope();
    fc.WriteArray("Targets", Wrap(targets, "'", "'"));
    fc.WritePopScope();
  }

  if (!outputGlobals) {

    FileContext& fc = context.bffFiles.main;
    fc.WriteComment("All");
    fc.WriteCommand("Alias", "'All'");
    fc.WritePushScope();
    fc.WriteArray("Targets", Wrap(usedConfigs, "'", "'"));
    fc.WritePopScope();
  }
}

void cmGlobalFastbuildGenerator::Detail::Generation::WriteBFFRebuildTarget(
  cmGlobalFastbuildGenerator* gg, FileContext& fc)
{
  std::vector<std::string> implicitDeps;
  for (std::vector<cmLocalGenerator*>::const_iterator i =
         gg->LocalGenerators.begin();
       i != gg->LocalGenerators.end(); ++i) {
    std::vector<std::string> const& lf = (*i)->GetMakefile()->GetListFiles();
    for (std::vector<std::string>::const_iterator fi = lf.begin();
         fi != lf.end(); ++fi) {
      implicitDeps.push_back(*fi);
    }
  }

  std::string outDir =
    std::string(
      gg->LocalGenerators[0]->GetMakefile()->GetHomeOutputDirectory()) +
#ifdef _WIN32
    '\\'
#else
    '/'
#endif
    ;

  implicitDeps.push_back(outDir + "CMakeCache.txt");

  std::sort(implicitDeps.begin(), implicitDeps.end());
  implicitDeps.erase(std::unique(implicitDeps.begin(), implicitDeps.end()),
                     implicitDeps.end());

  fc.WriteSectionHeader("re-run CMake to update fastbuild configs");
  fc.WriteCommand("Exec", cmGlobalFastbuildGenerator::Quote("rebuild-bff"));

  fc.WritePushScope();
  {
    cmLocalGenerator* lg = gg->LocalGenerators[0];

    std::ostringstream args;

    args << lg->ConvertToOutputFormat(cmSystemTools::GetCMakeCommand(),
                                      cmOutputConverter::SHELL)
         << " -H"
         << lg->ConvertToOutputFormat(lg->GetSourceDirectory(),
                                      cmOutputConverter::SHELL)
         << " -B"
         << lg->ConvertToOutputFormat(lg->GetBinaryDirectory(),
                                      cmOutputConverter::SHELL);

    fc.WriteArray("ExecInput",
                  cmGlobalFastbuildGenerator::Wrap(
                    gg->ConvertToFastbuildPath(implicitDeps), "'", "'"));
    fc.WriteVariable("ExecExecutable", cmSystemTools::GetCMakeCommand());
    fc.WriteVariable("ExecArguments",args.str());

    fc.WriteVariableBool("ExecIsGenerator",true);
    // Currently fastbuild doesn't support more than 1
    // output for a custom command (soon to change hopefully).
    // so only use the first one
    fc.WriteVariable("ExecOutput",gg->ConvertToFastbuildPath(outDir + "fbuild.bff"));
  }
  fc.WritePopScope();
}

//----------------------------------------------------------------------------
cmGlobalGeneratorFactory* cmGlobalFastbuildGenerator::NewFactory()
{
  return new cmGlobalGeneratorSimpleFactory<cmGlobalFastbuildGenerator>();
}

//----------------------------------------------------------------------------
cmGlobalFastbuildGenerator::cmGlobalFastbuildGenerator(cmake* cm)
  : cmGlobalCommonGenerator(cm)
{
#ifdef _WIN32
  cm->GetState()->SetWindowsShell(true);
#endif
  this->FindMakeProgramFile = "CMakeFastbuildFindMake.cmake";
  cm->GetState()->SetFastbuildMake(true);
}

//----------------------------------------------------------------------------
cmGlobalFastbuildGenerator::~cmGlobalFastbuildGenerator()
{
}

//----------------------------------------------------------------------------
cmLocalGenerator* cmGlobalFastbuildGenerator::CreateLocalGenerator(
  cmMakefile* makefile)
{
  return new cmLocalFastbuildGenerator(this, makefile);
}

//----------------------------------------------------------------------------
void cmGlobalFastbuildGenerator::EnableLanguage(
  std::vector<std::string> const& lang, cmMakefile* mf, bool optional)
{
  // Create list of configurations requested by user's cache, if any.
  this->cmGlobalGenerator::EnableLanguage(lang, mf, optional);

  if (!mf->GetDefinition("CMAKE_CONFIGURATION_TYPES")) {
    mf->AddCacheDefinition(
      "CMAKE_CONFIGURATION_TYPES", "Debug;Release;MinSizeRel;RelWithDebInfo",
      "Semicolon separated list of supported configuration types, "
      "only supports Debug, Release, MinSizeRel, and RelWithDebInfo, "
      "anything else will be ignored.",
      cmStateEnums::STRING);
  }
}

//----------------------------------------------------------------------------
void cmGlobalFastbuildGenerator::Generate()
{
  // Execute the standard generate process
  cmGlobalGenerator::Generate();

  Detail::Generation::GenerateRootBFF(this);
}

//----------------------------------------------------------------------------
void cmGlobalFastbuildGenerator::GenerateBuildCommand(
  std::vector<std::string>& makeCommand, const std::string& makeProgram,
  const std::string& /* projectName */, const std::string& projectDir,
  const std::string& targetName, const std::string& config, bool /*fast*/,
  bool /*verbose*/, std::vector<std::string> const& /*makeOptions*/)
{
  // A build command for fastbuild looks like this:
  // fbuild.exe [make-options] [-config projectName.bff] <target>-<config>

  // Setup make options
  std::vector<std::string> makeOptionsSelected;

  // Select the caller- or user-preferred make program
  std::string makeProgramSelected = this->SelectMakeProgram(makeProgram);

  // Select the target
  std::string targetSelected = targetName;
  // Note an empty target is a valid target (defaults to ALL anyway)
  if (targetSelected == "clean") {
    makeOptionsSelected.push_back("-clean");
    targetSelected = "";
  }

  // Select the config
  std::string configSelected = config;
  if (configSelected.empty()) {
    configSelected = "Debug";
  }

  // Select fastbuild target
  if (targetSelected.empty()) {
    targetSelected = configSelected;
  } else {
    targetSelected += "-" + configSelected;
  }

  // Hunt the fbuild.bff file in the directory above
  std::string configFile;
  if (!cmSystemTools::FileExists(projectDir + "fbuild.bff")) {
    configFile = cmSystemTools::FileExistsInParentDirectories(
      "fbuild.bff", projectDir.c_str(), "");
  }

  // Build the command
  makeCommand.push_back(makeProgramSelected);

  // Push in the make options
  makeCommand.insert(makeCommand.end(), makeOptionsSelected.begin(),
                     makeOptionsSelected.end());

  /*
  makeCommand.push_back("-config");
  makeCommand.push_back(projectName + ".bff");
  */

  makeCommand.push_back("-showcmds");
  makeCommand.push_back("-ide");

  if (!configFile.empty()) {
    makeCommand.push_back("-config");
    makeCommand.push_back(configFile);
  }

  // Add the target-config to the command
  if (!targetSelected.empty()) {
    makeCommand.push_back(targetSelected);
  }
}

//----------------------------------------------------------------------------
void cmGlobalFastbuildGenerator::AppendDirectoryForConfig(
  const std::string& prefix, const std::string& config,
  const std::string& suffix, std::string& dir)
{
  if (!config.empty()) {
    dir += prefix;
    dir += config;
    dir += suffix;
  }
}

//----------------------------------------------------------------------------
void cmGlobalFastbuildGenerator::ComputeTargetObjectDirectory(
  cmGeneratorTarget* gt) const
{
  // Compute full path to object file directory for this target.
  std::string dir;
  dir += gt->Makefile->GetCurrentBinaryDirectory();
  dir += "/";
  dir += gt->LocalGenerator->GetTargetDirectory(gt);
  dir += "/";
  gt->ObjectDirectory = dir;
}

//----------------------------------------------------------------------------
const char* cmGlobalFastbuildGenerator::GetCMakeCFGIntDir() const
{
  return FASTBUILD_DOLLAR_TAG "ConfigName" FASTBUILD_DOLLAR_TAG;
}

std::string cmGlobalFastbuildGenerator::ExpandCFGIntDir(
  const std::string& str, const std::string& config) const
{

  std::string replace = GetCMakeCFGIntDir();

  std::string tmp = str;
  for (std::string::size_type i = tmp.find(replace); i != std::string::npos;
       i = tmp.find(replace, i)) {
    tmp.replace(i, replace.size(), config);
    i += config.size();
  }
  return tmp;
}

//----------------------------------------------------------------------------
void cmGlobalFastbuildGenerator::GetDocumentation(cmDocumentationEntry& entry)
{
  entry.Name = cmGlobalFastbuildGenerator::GetActualName();
  entry.Brief = "Generates build.bff files.";
}

//----------------------------------------------------------------------------

std::string cmGlobalFastbuildGenerator::ConvertToFastbuildPath(
  const std::string& path) 
{
  return LocalGenerators[0]->ConvertToRelativePath(
    ((cmLocalCommonGenerator*)LocalGenerators[0])->GetWorkingDirectory(),
    path);
}

