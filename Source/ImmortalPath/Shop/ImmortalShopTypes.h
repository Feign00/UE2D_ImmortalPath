// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Alchemy/ImmortalAlchemyTypes.h"
#include "../Artifacts/ImmortalArtifactTypes.h"
#include "../Items/ImmortalEquipmentTypes.h"
#include "../Items/ImmortalMaterialTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ImmortalShopTypes.generated.h"

UENUM(BlueprintType)
enum class EImmortalShopProductType : uint8
{
	Equipment UMETA(DisplayName = "Equipment"),
	Material UMETA(DisplayName = "Material"),
	Pill UMETA(DisplayName = "Pill"),
	Artifact UMETA(DisplayName = "Artifact")
};

/** A persisted snapshot of one product. Equipment is generated at refresh time and never rerolled on purchase. */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalShopListing
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Shop")
	FGuid ListingId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Shop")
	EImmortalShopProductType ProductType = EImmortalShopProductType::Material;

	/** Stable material, pill or artifact ID. Equipment uses EquipmentItem instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Shop")
	FName ProductId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Shop", meta = (ClampMin = "1"))
	int32 BundleQuantity = 1;

	/** Total spirit-stone price for the complete bundle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Shop", meta = (ClampMin = "1"))
	int32 BundlePrice = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Shop")
	EImmortalPillQuality PillQuality = EImmortalPillQuality::Ordinary;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Shop")
	FImmortalEquipmentItem EquipmentItem;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Shop")
	bool bSoldOut = false;

	bool IsValid() const;
};

/** Daily stock and refresh audit fields are serialized as one versionable unit. */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalShopState
{
	GENERATED_BODY()

	/** Calendar key YYYYMMDD in the configured shop time zone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Shop")
	int32 RefreshDayKey = 0;

	/** Zero for the free daily stock; increases for paid manual refreshes on the same day. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Shop")
	int32 RefreshSerial = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Shop")
	int32 ManualRefreshCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Shop")
	TArray<FImmortalShopListing> Listings;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalShopTransactionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Shop") bool bSucceeded = false;
	UPROPERTY(BlueprintReadOnly, Category = "Shop") bool bAffordable = false;
	UPROPERTY(BlueprintReadOnly, Category = "Shop") bool bHadCapacity = false;
	UPROPERTY(BlueprintReadOnly, Category = "Shop") FGuid ListingId;
	/** Negative for purchases, positive for sales. */
	UPROPERTY(BlueprintReadOnly, Category = "Shop") int32 SpiritStoneDelta = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Shop") FText Message;
};

/** Pure catalog, refresh and valuation rules shared by runtime transactions and automation tests. */
UCLASS()
class IMMORTALPATH_API UImmortalShopLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Shop")
	static int32 GetDayKeyFromUtcTicks(int64 UtcTicks, int32 UtcOffsetMinutes = 480);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Shop")
	static int64 GetSecondsUntilNextRefresh(int64 UtcTicks, int32 UtcOffsetMinutes = 480);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Shop")
	static bool NeedsDailyRefresh(const FImmortalShopState& State, int32 CurrentDayKey);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Shop")
	static int32 GetManualRefreshCost(const FImmortalShopState& State);

	/** Generates three equipment, four material, two pill and one artifact listings when unlocked. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Shop")
	static FImmortalShopState GenerateStock(
		int32 QingyunStage,
		int32 RealmIndex,
		int32 MinorStage,
		int32 DayKey,
		int32 RefreshSerial = 0);

	static void NormalizeState(FImmortalShopState& State);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Shop|Pricing")
	static int32 GetEquipmentBuyPrice(const FImmortalEquipmentItem& Item);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Shop|Pricing")
	static int32 GetEquipmentSellPrice(const FImmortalEquipmentItem& Item);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Shop|Pricing")
	static int32 GetMaterialUnitBuyPrice(FName MaterialId);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Shop|Pricing")
	static int32 GetMaterialUnitSellPrice(FName MaterialId);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Shop|Pricing")
	static int32 GetPillUnitBuyPrice(FName PillId, EImmortalPillQuality Quality);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Shop|Pricing")
	static int32 GetArtifactBuyPrice(FName ArtifactId);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Shop|Display")
	static FText GetProductTypeText(EImmortalShopProductType ProductType);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Shop|Display")
	static FText GetListingDisplayName(const FImmortalShopListing& Listing);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Shop|Display")
	static FText GetListingDetailText(const FImmortalShopListing& Listing);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Shop|Display")
	static FLinearColor GetListingColor(const FImmortalShopListing& Listing);
};

