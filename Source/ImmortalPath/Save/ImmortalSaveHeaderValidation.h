// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/EngineVersion.h"
#include "Serialization/CustomVersion.h"
#include "UObject/ObjectVersion.h"

/** Bounded validation for the native UE 5.7 save format used by this project. */
namespace ImmortalSaveValidation
{
	struct FHeader
	{
		int64 PayloadOffset = 0;
		FPackageFileVersion PackageVersion;
		FEngineVersion EngineVersion;
		FCustomVersionContainer CustomVersions;
	};

	/**
	 * Reads only checked primitive bytes. Does not invoke the engine save-header
	 * deserializer, construct input-derived FNames, or load an input-named class.
	 * Failure clears Out. A valid header alone does not validate its payload.
	 */
	bool ReadHeader(const TArray<uint8>& Bytes, FHeader& Out);
}
