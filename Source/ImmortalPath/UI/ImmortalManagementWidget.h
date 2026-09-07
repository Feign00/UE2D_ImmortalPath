// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalManagementTypes.h"
#include "ImmortalManagementWidget.generated.h"

class AImmortalPlayerCharacter;
class UBorder;
class UButton;
class UImage;
class UTextBlock;
class UWidgetSwitcher;
class UTexture2D;

/**
	 * Inset 1707x320 management panel above the persistent desktop battle strip.
 *
 * The widget only changes UMG pages. It deliberately never pauses the world,
 * so the adventure map continues spawning monsters and resolving combat while
 * the player cultivates, manages equipment, visits the sect, and so on.
 */
UCLASS()
class IMMORTALPATH_API UImmortalManagementWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Management")
	void InitializeForPlayer(AImmortalPlayerCharacter* InPlayer);

	/** Adds or replaces one feature page inside the fixed content viewport. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Management")
	void RegisterFeaturePage(
		EImmortalManagementFeature Feature,
		UUserWidget* Page);

	/** Changes the switcher page in place; it never opens another modal. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Management")
	void ShowFeature(EImmortalManagementFeature Feature);

	/** Opens one complete management location and shows its in-world hotspots. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Management")
	void ShowScene(EImmortalManagementScene Scene);

	/** Resolves a Feature + mortal/spirit/immortal-world background. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Management")
	void RefreshTheme();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Management")
	EImmortalManagementFeature GetActiveFeature() const
	{
		return ActiveFeature;
	}

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Management")
	EImmortalManagementScene GetActiveScene() const
	{
		return ActiveScene;
	}

	/**
	 * Displays a transient management notification. While this interface is
	 * hidden, only the most recent notification is retained for the next visit.
	 */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Management")
	void QueueNotification(
		FText Message,
		FLinearColor Color,
		float Duration = 5.0f);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(
		const FGeometry& MyGeometry,
		float InDeltaTime) override;

private:
	UTexture2D* LoadOptionalTexture(const FString& AssetPath);

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UTexture2D>> ThemeTextureCache;

	void RequestFeature(EImmortalManagementFeature Feature);
	void RequestScene(EImmortalManagementScene Scene);
	void UpdateNavigationState();
	void UpdatePageHeader();
	void DisplayNotification(
		const FText& Message,
		const FLinearColor& Color,
		float Duration);
	void TryDisplayPendingNotification();
	void ClearNotification();
	bool IsManagementVisible() const;
	UWidget* BuildHomePage();
	UWidget* BuildSceneHub(EImmortalManagementScene Scene);
	UWidget* BuildMissingPage();
	UButton* AddNavigationButton(
		EImmortalManagementFeature Feature,
		const FText& Label);
	UButton* AddSceneButton(
		EImmortalManagementScene Scene,
		const FText& Label);
	UButton* AddSceneHotspot(
		EImmortalManagementFeature Feature,
		const FText& Label,
		const FVector2D& Position,
		const FVector2D& Size);

	UFUNCTION()
	void HandleSectSceneClicked();

	UFUNCTION()
	void HandleMarketSceneClicked();

	UFUNCTION()
	void HandleCaveSceneClicked();

	UFUNCTION()
	void HandleAscensionClicked();

	UFUNCTION()
	void HandleAdventureSceneClicked();

	UFUNCTION()
	void HandleBackToSceneClicked();

	UFUNCTION()
	void HandleHomeClicked();

	UFUNCTION()
	void HandleCultivationClicked();

	UFUNCTION()
	void HandleInventoryClicked();

	UFUNCTION()
	void HandleAlchemyClicked();

	UFUNCTION()
	void HandleCraftingClicked();

	UFUNCTION()
	void HandleArtifactClicked();

	UFUNCTION()
	void HandleTechniqueClicked();

	UFUNCTION()
	void HandleCharacterBuildClicked();

	UFUNCTION()
	void HandleShopClicked();

	UFUNCTION()
	void HandleMapClicked();

	UFUNCTION()
	void HandleQuestClicked();

	UFUNCTION()
	void HandleCaveClicked();

	UFUNCTION()
	void HandleFarmingClicked();

	UFUNCTION()
	void HandleSectClicked();

	UFUNCTION()
	void HandleWorldBossClicked();

	UFUNCTION()
	void HandleEndlessDungeonClicked();

	UFUNCTION()
	void HandlePetClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleReturnToAdventureClicked();

	UPROPERTY(Transient)
	TWeakObjectPtr<AImmortalPlayerCharacter> Player;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ThemePlaceholder;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ThemeImage;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PageTitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ThemeStatusText;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetSwitcher> PageSwitcher;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetSwitcher> SceneHubSwitcher;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MissingPageText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> NotificationBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NotificationText;

	UPROPERTY(Transient)
	TMap<EImmortalManagementFeature, TObjectPtr<UWidget>> RegisteredPages;

	UPROPERTY(Transient)
	TMap<EImmortalManagementFeature, TObjectPtr<UUserWidget>> RegisteredPageSources;

	UPROPERTY(Transient)
	TMap<EImmortalManagementFeature, TObjectPtr<UButton>> NavigationButtons;

	UPROPERTY(Transient)
	TMap<EImmortalManagementScene, TObjectPtr<UWidget>> SceneHubPages;

	UPROPERTY(Transient)
	TMap<EImmortalManagementScene, TObjectPtr<UButton>> SceneButtons;

	UPROPERTY(Transient)
	TObjectPtr<UButton> BackToSceneButton;

	EImmortalManagementFeature ActiveFeature =
		EImmortalManagementFeature::Home;
	EImmortalManagementScene ActiveScene =
		EImmortalManagementScene::SectSanctuary;
	FText PendingNotificationText;
	FLinearColor PendingNotificationColor = FLinearColor::White;
	float PendingNotificationDuration = 5.0f;
	bool bHasPendingNotification = false;
	double NotificationExpirySeconds = 0.0;
	uint8 LastThemeRealm = MAX_uint8;
	float ThemeRefreshAccumulator = 0.0f;
};
