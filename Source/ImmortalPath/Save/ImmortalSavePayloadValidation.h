// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UImmortalPathSaveGame;

namespace ImmortalSaveValidation
{
	struct FHeader;

	/**
	 * Bounded, schema-directed decoding of this project's native value snapshot.
	 * A null Target performs the same complete validation without interning any
	 * input names or allocating object/container values. No engine deserializer
	 * is ever given untrusted property tags or container lengths.
	 */
	bool ReadPayload(const TArray<uint8>& Bytes, const FHeader& Header,
		UImmortalPathSaveGame* Target = nullptr, FString* OutReason = nullptr);
}
