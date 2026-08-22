// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "ImmortalPaperImportCommandlet.generated.h"

/**
 * Deterministic, headless Paper2D importer used for project-owned PNG source art.
 *
 * Run with:
 *   UnrealEditor-Cmd.exe <project> -run=ImmortalPaperImport
 *     -ImportSettings=<absolute-or-project-relative-json>
 */
UCLASS()
class UImmortalPaperImportCommandlet final : public UCommandlet
{
	GENERATED_BODY()

public:
	UImmortalPaperImportCommandlet();

	virtual int32 Main(const FString& Params) override;
};
