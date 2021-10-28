// Copyright (C) 2018-2020 - DevSH Graphics Programming Sp. z O.O.
// This file is part of the "Nabla Engine".
// For conditions of distribution and use, see copyright notice in nabla.h

#ifndef _RAYTRACER_COMMAND_LINE_HANDLER_
#define _RAYTRACER_COMMAND_LINE_HANDLER_

#include <iostream>
#include <cstdio>
#include <chrono>
#include "nabla.h"
#include "nbl/core/core.h"
#include "nbl/ext/MitsubaLoader/CMitsubaLoader.h"

constexpr std::string_view helpMessage = R"(

Parameters:
-SCENE=sceneMitsubaXMLPathOrZipAndXML
-SCREENSHOT_OUTPUT=exrScreenShotOutputFilePath
-TERMINATE

Description and usage: 

-SCENE:
	some/path extra/path which will make it skip the file choose dialog
	
-SCREENSHOT_OUTPUT:
	output folder path for exr screenshots
	
-TERMINATE:
	which will make the app stop when the required amount of samples has been renderered (its in the Mitsuba Scene metadata) and obviously take screenshot when quitting
	
Example Usage :
	raytracedao.exe -SCENE=../../media/kitchen.zip scene.xml -TERMINATE -SCREENSHOT_OUTPUT_FOLDER=C:\\Users\\UserName\\Documents\\My-ScreenShots
Alternative:
	raytracedao.exe -SCENE ../../media/kitchen.zip scene.xml -TERMINATE -SCREENSHOT_OUTPUT_FOLDER C:\\Users\\UserName\\Documents\\My-ScreenShots
)";
 

constexpr std::string_view SCENE_VAR_NAME						= "SCENE";
constexpr std::string_view SCREENSHOT_OUTPUT_FOLDER_VAR_NAME	= "SCREENSHOT_OUTPUT_FOLDER";
constexpr std::string_view TERMINATE_VAR_NAME					= "TERMINATE";

constexpr uint32_t MaxRayTracerCommandLineArgs = 8;

enum RaytracerExampleArguments
{
	REA_SCENE,
	REA_SCREENSHOT_OUTPUT_FOLDER,
	REA_TERMINATE,
	REA_COUNT,
};

using variablesType = std::unordered_map<RaytracerExampleArguments, std::optional<std::vector<std::string>>>;

class CommandLineHandler
{
	public:

		CommandLineHandler(const std::vector<std::string>& argv);

		auto& getSceneDirectory() const
		{
			return sceneDirectory;
		}
		
		auto& getOutputScreenshotsFolderPath() const
		{
			return outputScreenshotsFolderPath;
		}

		auto& getTerminate() const
		{
			return terminate;
		}

	private:

		void initializeMatchingMap()
		{
			rawVariables[REA_SCENE];
			rawVariables[REA_SCREENSHOT_OUTPUT_FOLDER];
			rawVariables[REA_TERMINATE];
		}

		RaytracerExampleArguments getMatchedVariableMapID(const std::string& variableName)
		{
			if (variableName == SCENE_VAR_NAME)
				return REA_SCENE;
			else if (variableName == SCREENSHOT_OUTPUT_FOLDER_VAR_NAME)
				return REA_SCREENSHOT_OUTPUT_FOLDER;
			else if (variableName == TERMINATE_VAR_NAME)
				return REA_TERMINATE;
			else
				return REA_COUNT;
		}

		bool validateParameters();

		void performFinalAssignmentStepForUsefulVariables()
		{
			if(rawVariables[REA_SCENE].has_value())
				sceneDirectory = rawVariables[REA_SCENE].value();
			if(rawVariables[REA_SCREENSHOT_OUTPUT_FOLDER].has_value())
			{
				const auto& screenShotPathVector = rawVariables[REA_SCREENSHOT_OUTPUT_FOLDER].value();
				if(screenShotPathVector.size())
					outputScreenshotsFolderPath = screenShotPathVector[0];
			}
			if(rawVariables[REA_TERMINATE].has_value())
				terminate = true;
		}

		variablesType rawVariables;

		// Loaded from CMD
		std::vector<std::string> sceneDirectory; // [0] zip [1] optional xml in zip
		std::string outputScreenshotsFolderPath;
		bool terminate = false;
};

#endif // _DENOISER_TONEMAPPER_COMMAND_LINE_HANDLER_