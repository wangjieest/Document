#include <Render.h>

class ProgressReporter : public Object, public virtual IProgressReporter
{
private:
	static const vint		MaxProgress = 500;
	static const vint		MaxProgressPerLine = 50;

	vint					currentPhase = 0;
	vint					currentProgress = 0;

	void UpdateProgressInCurrentPhase(vint progress)
	{
		if (progress > currentProgress)
		{
			for (vint i = currentProgress + 1; i <= progress; i++)
			{
				if (i % MaxProgressPerLine == 1)
				{
					Console::Write(L"     " + itow(currentPhase) + L": ");
				}
				Console::Write(L"*");
				if (i % MaxProgressPerLine == 0)
				{
					Console::WriteLine(L"");
				}
			}
			currentProgress = progress;
		}
	}

public:
	void FinishPhase()
	{
		UpdateProgressInCurrentPhase(MaxProgress);
		currentPhase++;
		currentProgress = 0;
	}

	void OnProgress(vint phase, vint totalPhases, vint position, vint length)
	{
		if (phase != currentPhase)
		{
			for (vint i = currentPhase; i < phase; i++)
			{
				FinishPhase();
			}
		}

		double ratio = (double)position / length;
		if (ratio < 0) ratio = 0;
		if (ratio > 1) ratio = 1;
		UpdateProgressInCurrentPhase((vint)(ratio*MaxProgress));
	}
};

void IndexCppCode(
	// input
	FileGroupConfig&		fileGroups,					// source folder (ending with FilePath::Delimiter) -> category name
	File					preprocessedFile,			// .I file generated by cl.exe
	Ptr<RegexLexer>			lexer,						// C++ lexical analyzer

	// output
	FilePath				pathPreprocessed,			// cache: preprocessed file
	FilePath				pathInput,					// cache: compacted preprocessed file, removing all empty, space or # lines
	FilePath				pathMapping,				// cache: line mapping between pathPreprocessed and pathInput
	Folder					folderOutput				// folder containing generated HTML files
)
{
	if (!folderOutput.Exists())
	{
		folderOutput.Create(true);
	}

	Console::WriteLine(preprocessedFile.GetFilePath().GetFullPath());
	Console::WriteLine(L"    Preprocessing");
	PreprocessedFileToCompactCodeAndMapping(
		lexer,
		preprocessedFile.GetFilePath(),
		pathPreprocessed,
		pathInput,
		pathMapping
	);

	Console::WriteLine(L"    Compiling");
	IndexResult indexResult;
	{
		ProgressReporter progressReporter;
		Compile(
			lexer,
			pathInput,
			indexResult,
			&progressReporter
		);
		progressReporter.FinishPhase();
	}

	Console::WriteLine(L"    Generating HTML");
	auto global = Collect(
		lexer,
		pathPreprocessed,
		pathInput,
		pathMapping,
		indexResult
	);

	for (vint i = 0; i < global->fileLines.Keys().Count(); i++)
	{
		auto flr = global->fileLines.Values()[i];
		GenerateFile(global, flr, indexResult, folderOutput.GetFilePath() / (flr->htmlFileName + L".html"));
	}

	List<WString> sourcePrefixes;
	{
		CopyFrom(
			sourcePrefixes,
			From(fileGroups)
				.Select([](Tuple<WString, WString> fileGroup)
				{
					return fileGroup.f0;
				})
			);

		SortedList<FilePath> sdkPaths;
		for (vint i = 0; i < global->fileLines.Count(); i++)
		{
			auto filePath = global->fileLines.Values()[i]->filePath;
			if (!From(sourcePrefixes)
				.Any([=](const WString& prefix)
				{
					return INVLOC.StartsWith(filePath.GetFullPath(), prefix, Locale::Normalization::IgnoreCase);
				}))
			{
				auto sdkPath = filePath.GetFolder();
				if (!sdkPaths.Contains(sdkPath))
				{
					sdkPaths.Add(sdkPath);
					fileGroups.Add({ sdkPath.GetFullPath() + FilePath::Delimiter, L"In SDK: " + sdkPath.GetFullPath() });
				}
			}
		}
	}

	GenerateFileIndex(global, folderOutput.GetFilePath() / L"FileIndex.html", fileGroups);
	GenerateSymbolIndex(global, indexResult, folderOutput.GetFilePath() / L"SymbolIndex.html", fileGroups);
}

/***********************************************************************
Main

Set root folder which contains UnitTest_Cases.vcxproj
Open http://127.0.0.1:8080/Calculator.i.Output/FileIndex.html
***********************************************************************/

int main()
{
	List<File> preprocessedFiles;
	preprocessedFiles.Add(File(L"../UnitTest_Cases/Calculator.i"));
	//preprocessedFiles.Add(File(L"../UnitTest_Cases/STL.i"));

	Console::WriteLine(L"Cleaning ...");
	FOREACH(File, file, preprocessedFiles)
	{
		Folder folderOutput(file.GetFilePath().GetFullPath() + L".Output");
		if (folderOutput.Exists())
		{
			folderOutput.Delete(true);
		}
	}
	auto lexer = CreateCppLexer();

	FOREACH(File, file, preprocessedFiles)
	{
		Folder folderOutput(file.GetFilePath().GetFullPath() + L".Output");
		auto pathPreprocessed = folderOutput.GetFilePath() / L"Preprocessed.cpp";
		auto pathInput = folderOutput.GetFilePath() / L"Input.cpp";
		auto pathMapping = folderOutput.GetFilePath() / L"Mapping.bin";

		FileGroupConfig fileGroups;
		fileGroups.Add({ file.GetFilePath().GetFolder().GetFullPath() + FilePath::Delimiter, L"Source Code of this Project" });
		IndexCppCode(fileGroups, file, lexer, pathPreprocessed, pathInput, pathMapping, folderOutput);
	}

	//{
	//	Folder folderOutput(L"../../../.Output/Import/Preprocessed.txt.Output");
	//	auto pathPreprocessed = folderOutput.GetFilePath() / L"Preprocessed.cpp";
	//	auto pathInput = folderOutput.GetFilePath() / L"Input.cpp";
	//	auto pathMapping = folderOutput.GetFilePath() / L"Mapping.bin";

	//	if (!folderOutput.Exists())
	//	{
	//		folderOutput.Create(false);
	//	}

	//	Console::WriteLine(L"Preprocessing Preprocessed.txt ...");
	//	CleanUpPreprocessFile(
	//		lexer,
	//		folderOutput.GetFilePath() / L"../Preprocessed.txt",
	//		pathPreprocessed,
	//		pathInput,
	//		pathMapping
	//	);
	//}

	return 0;
}