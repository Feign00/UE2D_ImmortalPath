#include "ImmortalPlayerCharacter.h"
#if !UE_BUILD_SHIPPING
#include "../UI/ImmortalManagementWidget.h"
#include "../UI/ImmortalDesktopPanelLayout.h"
#include "../UI/ImmortalAlchemyWidget.h"
#include "../UI/ImmortalCraftingWidget.h"
#include "../UI/ImmortalCultivationWidget.h"
#include "Components/ProgressBar.h"
#include "../UI/ImmortalSectWidget.h"
#include "../UI/ImmortalFarmingWidget.h"
#include "../UI/ImmortalInventoryWidget.h"
#include "../UI/ImmortalInventorySlotWidget.h"
#include "Components/ButtonSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#endif

void AImmortalPlayerCharacter::RunDesktopPanelFixture()
{
#if !UE_BUILD_SHIPPING
	const bool bBuildings = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestDesktopBuildings"));
	const bool bInventoryPreview = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestInventoryPreview"));
	const bool bInventoryFixture = bInventoryPreview || FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestInventoryPanels"));
	const bool bLayoutPreview = bInventoryPreview || FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestFeatureLayoutPreview"));
	if (!bBuildings && !bLayoutPreview && !bInventoryFixture && !FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestDesktopPanels"))) return;
	FString UserDir;
	if (!FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir)
		|| !FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(UserDir),
			FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved/Automation/DesktopPanels")))))
	{
		UE_LOG(LogTemp, Error, TEXT("Desktop panel fixture refused: isolated UserDir required.")); return;
	}
	if (bInventoryFixture)
	{
		// This fixture is intentionally unreachable in Shipping and outside the isolated directory.
		bAutoEquipNewItems = false;
		InventoryItems.Reset();
		for (int32 Index = 0; Index < GetInventoryCapacity(); ++Index)
		{
			auto Item = UImmortalEquipmentLibrary::GenerateCraftedEquipment(1 + Index,
				static_cast<EImmortalEquipmentSlot>(Index % 9), static_cast<EImmortalEquipmentQuality>(Index % 5));
			Item.bLocked = Index <= 1;
			InventoryItems.Add(Item);
		}
		InventoryItems[0].DisplayName = TEXT("青云镇岳流光长剑·长名称与多词条布局验证");
		++EquipmentInventoryRevision;
	}
	if (bLayoutPreview)
	{
		// Isolated, non-shipping visual fixture: enough rows to exercise the pill scroll area.
		for (FName Recipe : UImmortalAlchemyLibrary::GetKnownRecipeIds())
		{
			AddPillInternal(Recipe, EImmortalPillQuality::Ordinary, 1);
			AddPillInternal(Recipe, EImmortalPillQuality::Exceptional, 1);
		}
		++PillInventoryRevision;
		InvulnerableUntilTime = GetWorld()->GetTimeSeconds() + 3600;
		UE_LOG(LogTemp, Display, TEXT("Feature layout preview ready: %d pill stacks; manual navigation; isolated save."), PillInventory.Num());
		return;
	}
	const auto Failures = MakeShared<int32>(0);
	const auto Baseline = MakeShared<FVector2D>();
	const auto BaseSize = MakeShared<FIntPoint>();
	const auto Check = [Failures](const TCHAR* Name, bool Passed)
	{
		UE_LOG(LogTemp, Display, TEXT("Desktop panel check: %s passed=%s"), Name, Passed ? TEXT("true") : TEXT("false"));
		if (!Passed) ++*Failures;
	};
	const auto At = [this](float Time, TFunction<void()> Work)
	{
		FTimerHandle Timer; GetWorldTimerManager().SetTimer(Timer,
			FTimerDelegate::CreateWeakLambda(this, [Work] { Work(); }), Time, false);
	};
	const auto Shot = [](const FString& Name)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),
			TEXT("Screenshots/DesktopPanel_") + Name + TEXT(".png")), true, false);
	};
	At(1.6f, [this, Baseline, BaseSize]
	{
		InvulnerableUntilTime = GetWorld()->GetTimeSeconds() + 120;
		auto* PC = Cast<APlayerController>(GetController());
		PC->GetViewportSize(BaseSize->X, BaseSize->Y);
		PC->ProjectWorldLocationToScreen(GetActorLocation(), *Baseline);
		OpenManagementInterface();
	});
	At(2.6f, [this, Baseline, BaseSize, Check, Shot]
	{
		auto* PC = Cast<APlayerController>(GetController()); FIntPoint Size; FVector2D Point;
		PC->GetViewportSize(Size.X, Size.Y); PC->ProjectWorldLocationToScreen(GetActorLocation(), Point);
		UE_LOG(LogTemp, Display, TEXT("Desktop panel anchor: base=%dx%d point=%.2f,%.2f expanded=%dx%d point=%.2f,%.2f"),
			BaseSize->X, BaseSize->Y, Baseline->X, Baseline->Y, Size.X, Size.Y, Point.X, Point.Y);
		Check(TEXT("native window expanded upward"), Size.Y > BaseSize->Y && Size.X == BaseSize->X);
		Check(TEXT("battle desktop position preserved"), FMath::Abs(Point.X - Baseline->X) < 2
			&& FMath::Abs(Point.Y - Baseline->Y - (Size.Y - BaseSize->Y)) < 2);
		Shot(TEXT("Home"));
	});
	const EImmortalManagementFeature Pages[] = { EImmortalManagementFeature::Cultivation,
		EImmortalManagementFeature::Inventory, EImmortalManagementFeature::Alchemy, EImmortalManagementFeature::Crafting,
		EImmortalManagementFeature::Shop, EImmortalManagementFeature::Farming, EImmortalManagementFeature::Sect,
		EImmortalManagementFeature::Settings };
	const EImmortalManagementFeature BuildingPages[] = { EImmortalManagementFeature::Cultivation,
		EImmortalManagementFeature::Sect, EImmortalManagementFeature::Alchemy, EImmortalManagementFeature::Crafting,
		EImmortalManagementFeature::Cave, EImmortalManagementFeature::Farming, EImmortalManagementFeature::Shop };
	const TCHAR* Tokens[] = {TEXT("Cultivation"), TEXT("Sect"), TEXT("Alchemy"), TEXT("Crafting"), TEXT("Cave"), TEXT("Farming"), TEXT("Shop")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Pages); ++Index)
	{
		if (bBuildings && Index >= UE_ARRAY_COUNT(BuildingPages)) break;
		const auto Page = bBuildings ? BuildingPages[Index] : Pages[Index];
		const FString Token = bBuildings ? Tokens[Index] : TEXT("");
		At(3.5f + Index, [this, Page, Token, bBuildings, Check] {
			if (bBuildings)
			{
				PlayerManagementWidget->ShowScene(Page == EImmortalManagementFeature::Shop
					? EImmortalManagementScene::MarketTown : EImmortalManagementScene::SectSanctuary);
				UImage* Art = Cast<UImage>(PlayerManagementWidget->WidgetTree->FindWidget(
					FName(*(TEXT("ManagementBuilding_") + Token))));
				Check(TEXT("building illustration loaded and visible"), Art && Art->GetBrush().GetResourceObject()
					&& Art->GetVisibility() == ESlateVisibility::HitTestInvisible);
				UButton* Button = Cast<UButton>(PlayerManagementWidget->WidgetTree->FindWidget(
					FName(*(TEXT("ManagementHotspot_") + Token))));
				if (Button) Button->OnClicked.Broadcast();
			}
			else OpenManagementFeature(Page);
			Check(TEXT("page switch keeps live combat"), !UGameplayStatics::IsGamePaused(this)
				&& GetWorldTimerManager().IsTimerActive(AutoAttackTimerHandle)
				&& PlayerManagementWidget->GetActiveFeature() == Page);
		});
		At(4.0f + Index, [this, Shot, Index, Page, Check] {
			if (Page == EImmortalManagementFeature::Cultivation)
			{
				PlayerCultivationWidget->RefreshFromPlayer();
				UWidgetTree* Tree = PlayerCultivationWidget->WidgetTree;
				const USizeBox* Root = Cast<USizeBox>(Tree->RootWidget);
				Check(TEXT("cultivation uses a full 1600x600 page"), Root && Root->GetWidthOverride() == 1600 && Root->GetHeightOverride() == 600);
				bool bBounded = true, bReadable = true;
				Tree->ForEachWidget([&](UWidget* Widget)
				{
					if (const UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot))
					{
						const auto Start = Slot->GetPosition(), End = Start + Slot->GetSize();
						const auto ParentSize = Widget->GetParent()->GetCachedGeometry().GetLocalSize();
						const bool bInside = Start.X >= 0 && Start.Y >= 0 && End.X <= ParentSize.X + 1 && End.Y <= ParentSize.Y + 1;
						if (!bInside) UE_LOG(LogTemp, Display, TEXT("Feature layout overflow: %s end=%s parent=%s"),
							*Widget->GetName(), *End.ToString(), *ParentSize.ToString());
						bBounded &= bInside;
					}
					if (const UTextBlock* Text = Cast<UTextBlock>(Widget)) bReadable &= Text->GetFont().Size >= 18;
				});
				Check(TEXT("cultivation text is at least 18pt and controls are bounded"), bBounded && bReadable);
				const UProgressBar* Progress = Cast<UProgressBar>(Tree->FindWidget(TEXT("CultivationProgressBar")));
				Check(TEXT("cultivation has a large valid progress bar"), Progress && Progress->GetPercent() >= 0 && Progress->GetPercent() <= 1
					&& Progress->GetCachedGeometry().GetLocalSize().X >= 948);
				const bool WasRecoveryRequired = bDeathCultivationRecoveryRequired;
				bDeathCultivationRecoveryRequired = true;
				PlayerCultivationWidget->RefreshFromPlayer();
				bool bLocked = true;
				for (const TCHAR* Name : {TEXT("CultivationReturnHome"), TEXT("CultivationCloseToHome"), TEXT("CultivationOpenAscension")})
				{
					const UButton* Action = Cast<UButton>(Tree->FindWidget(FName(Name)));
					bLocked &= Action && !Action->GetIsEnabled();
				}
				Check(TEXT("cultivation recovery visibly disables leaving and ascension"), bLocked);
				bDeathCultivationRecoveryRequired = WasRecoveryRequired;
				PlayerCultivationWidget->RefreshFromPlayer();
			}
			if (Page == EImmortalManagementFeature::Inventory)
			{
				UWidgetTree* Tree = PlayerInventoryWidget->WidgetTree;
				USizeBox* Root = Cast<USizeBox>(Tree->RootWidget);
				UCanvasPanel* Doll = Cast<UCanvasPanel>(Tree->FindWidget(TEXT("EquipmentGrid")));
				UImage* Portrait = Cast<UImage>(Tree->FindWidget(TEXT("InventoryPortrait")));
				Check(TEXT("inventory uses enlarged paper doll with ten slots and portrait"), Root && Root->GetHeightOverride() == 600
					&& Doll && Doll->GetChildrenCount() == 10 && Portrait && Portrait->GetBrush().GetResourceObject());
				UUniformGridPanel* Bag = Cast<UUniformGridPanel>(Tree->FindWidget(TEXT("BackpackGrid")));
				bool bGrid = Bag && Bag->GetChildrenCount() >= GetInventoryCapacity();
				if (Bag) for (UWidget* Cell : Bag->GetAllChildren())
				{
					UUniformGridSlot* Slot = Cast<UUniformGridSlot>(Cell->Slot);
					bGrid &= Slot && Slot->GetColumn() < 7 && Cell->GetDesiredSize().X >= 84;
				}
				Check(TEXT("inventory grid has seven columns and same-frame sized cells"), bGrid);
				bool bFill = Bag && Bag->GetChildrenCount() > 0;
				if (Bag) for (UWidget* Cell : Bag->GetAllChildren())
				{
					UImmortalInventorySlotWidget* ItemCell = Cast<UImmortalInventorySlotWidget>(Cell);
					UWidget* Layers = ItemCell ? ItemCell->WidgetTree->FindWidget(TEXT("InventorySlotLayers")) : nullptr;
					UWidget* Frame = ItemCell ? ItemCell->WidgetTree->FindWidget(TEXT("InventoryQualityFrame")) : nullptr;
					const UButtonSlot* Content = Layers ? Cast<UButtonSlot>(Layers->Slot) : nullptr;
					const UOverlaySlot* Border = Frame ? Cast<UOverlaySlot>(Frame->Slot) : nullptr;
					bFill &= Content && Border && Content->GetHorizontalAlignment() == HAlign_Fill
						&& Content->GetVerticalAlignment() == VAlign_Fill && Border->GetHorizontalAlignment() == HAlign_Fill
						&& Border->GetVerticalAlignment() == VAlign_Fill;
				}
				Check(TEXT("inventory cell content and quality outline fill their slots"), bFill);
				bool bWithinPanel = true;
				Tree->ForEachWidget([&](UWidget* Widget)
				{
					if (const UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot))
					{
						const auto End = Slot->GetPosition() + Slot->GetSize();
						const auto ParentSize = Widget->GetParent()->GetCachedGeometry().GetLocalSize();
						bWithinPanel &= Slot->GetPosition().X >= 0 && Slot->GetPosition().Y >= 0
							&& End.X <= ParentSize.X + 1 && End.Y <= ParentSize.Y + 1;
					}
				});
				Check(TEXT("inventory controls stay inside parent bounds"), bWithinPanel);
				if (!InventoryItems.IsEmpty())
				{
					const FGuid Id = InventoryItems[0].ItemId;
					PlayerInventoryWidget->HandleSlotSelected(Id);
					SetEquipmentLocked(Id, true); PlayerInventoryWidget->RefreshFromPlayer();
					UButton* SellLocked = Cast<UButton>(Tree->FindWidget(TEXT("InventorySellSelectedButton")));
					UButton* DismantleLocked = Cast<UButton>(Tree->FindWidget(TEXT("InventoryDismantleSelectedButton")));
					Check(TEXT("selected locked item disables sale and dismantle"), SellLocked && DismantleLocked
						&& !SellLocked->GetIsEnabled() && !DismantleLocked->GetIsEnabled());
					SetEquipmentLocked(Id, false);
				}
				const auto Before = InventoryItems;
				InventoryItems.Reset(); ++EquipmentInventoryRevision;
				PlayerInventoryWidget->RefreshFromPlayer();
				UButton* Sell = Cast<UButton>(Tree->FindWidget(TEXT("InventorySellSelectedButton")));
				Check(TEXT("empty backpack disables destructive action"), Sell && !Sell->GetIsEnabled());
				InventoryItems = Before; ++EquipmentInventoryRevision;
				PlayerInventoryWidget->RefreshFromPlayer();
			}
			if (Page == EImmortalManagementFeature::Sect || Page == EImmortalManagementFeature::Farming || Page == EImmortalManagementFeature::Alchemy)
			{
				UUserWidget* ReflowPage = Page == EImmortalManagementFeature::Sect
					? static_cast<UUserWidget*>(PlayerSectWidget.Get()) : (Page == EImmortalManagementFeature::Alchemy
						? static_cast<UUserWidget*>(PlayerAlchemyWidget.Get()) : static_cast<UUserWidget*>(PlayerFarmingWidget.Get()));
				USizeBox* Root = Cast<USizeBox>(ReflowPage->WidgetTree->RootWidget);
				Check(TEXT("feature page matches authored content height"), Root && Root->GetHeightOverride() == 600);
				if (Page == EImmortalManagementFeature::Alchemy)
				{
					const UImage* Art = Cast<UImage>(ReflowPage->WidgetTree->FindWidget(TEXT("AlchemyFurnaceArt")));
					Check(TEXT("alchemy cauldron uses imported art"), Art && Art->GetBrush().GetResourceObject());
					const UVerticalBox* Recipes = Cast<UVerticalBox>(ReflowPage->WidgetTree->FindWidget(TEXT("RecipeList")));
					bool bIcons = Recipes && Recipes->GetChildrenCount() == UImmortalAlchemyLibrary::GetKnownRecipeIds().Num();
					if (Recipes) for (UWidget* Entry : Recipes->GetAllChildren())
					{
						const UUserWidget* Row = Cast<UUserWidget>(Entry);
						const UImage* Icon = Row ? Cast<UImage>(Row->WidgetTree->FindWidget(TEXT("RecipeArt"))) : nullptr;
						bIcons &= Icon && Icon->GetBrush().GetResourceObject() && Entry->GetDesiredSize().Y >= 90;
					}
					Check(TEXT("alchemy recipes have illustrated large rows"), bIcons);
					const auto SavedPills = PillInventory;
					PillInventory.Reset(); ++PillInventoryRevision;
					PlayerAlchemyWidget->RefreshFromPlayer();
					const UUniformGridPanel* EmptyGrid = Cast<UUniformGridPanel>(ReflowPage->WidgetTree->FindWidget(TEXT("PillGrid")));
					const UButton* Use = Cast<UButton>(ReflowPage->WidgetTree->FindWidget(TEXT("UsePillButton")));
					Check(TEXT("empty alchemy inventory keeps five slots and disables use"), EmptyGrid && EmptyGrid->GetChildrenCount() == 5 && Use && !Use->GetIsEnabled());
					PillInventory = SavedPills; ++PillInventoryRevision;
					PlayerAlchemyWidget->RefreshFromPlayer();
				}
				if (Page == EImmortalManagementFeature::Sect)
				{
					bool bEmblems = true;
					for (int32 Sect = 0; Sect < 4; ++Sect)
					{
						const UImage* Art = Cast<UImage>(ReflowPage->WidgetTree->FindWidget(FName(*FString::Printf(TEXT("SectEmblem%d"), Sect))));
						bEmblems &= Art && Art->GetBrush().GetResourceObject() && Art->GetBrush().DrawAs == ESlateBrushDrawType::Image;
					}
					Check(TEXT("all four sect emblems use the imported atlas"), bEmblems);
					bool bProgress = true;
					for (int32 Task = 0; Task < 3; ++Task)
					{
						const UProgressBar* Bar = Cast<UProgressBar>(ReflowPage->WidgetTree->FindWidget(FName(*FString::Printf(TEXT("SectTaskProgress%d"), Task))));
						bProgress &= Bar && Bar->GetCachedGeometry().GetLocalSize().X >= 300 && Bar->GetPercent() >= 0 && Bar->GetPercent() <= 1;
					}
					Check(TEXT("sect tasks have three bounded visible progress bars"), bProgress);
				}
				if (Page == EImmortalManagementFeature::Farming)
				{
					bool bImages = true;
					for (int32 Plot = 0; Plot < 6; ++Plot)
					{
						const UImage* Art = Cast<UImage>(ReflowPage->WidgetTree->FindWidget(FName(*FString::Printf(TEXT("FarmingPlotImage%d"), Plot))));
						bImages &= Art && Art->GetBrush().GetResourceObject() && Art->GetVisibility() == ESlateVisibility::HitTestInvisible;
					}
					Check(TEXT("all six farming cards render imported state art"), bImages);
				}
				bool bBounded = true;
				bool bReadable = true;
				ReflowPage->WidgetTree->ForEachWidget([&](UWidget* Widget)
				{
					if (UTextBlock* Text = Cast<UTextBlock>(Widget))
						if (Text->GetVisibility() != ESlateVisibility::Collapsed) bReadable &= Text->GetFont().Size >= 14;
					if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot))
					{
						const FVector2D ParentSize = Widget->GetParent()->GetCachedGeometry().GetLocalSize();
						const FVector2D Start = Slot->GetPosition(), End = Start + Slot->GetSize();
						const bool bInside = Start.X >= 0 && Start.Y >= 0 && End.X <= ParentSize.X + 1 && End.Y <= ParentSize.Y + 1;
						if (!bInside) UE_LOG(LogTemp, Display, TEXT("Feature layout overflow: %s end=%s parent=%s"),
							*Widget->GetName(), *End.ToString(), *ParentSize.ToString());
						bBounded &= bInside;
					}
				});
				Check(TEXT("reflow text uses at least 14pt without out-of-bounds controls"), bReadable && bBounded);
			}
			UUserWidget* DetailPage = nullptr;
			const TCHAR* ListName = TEXT("");
			if (Page == EImmortalManagementFeature::Alchemy)
			{
				PlayerAlchemyWidget->RefreshFromPlayer();
				DetailPage = PlayerAlchemyWidget; ListName = TEXT("RecipeList");
			}
			else if (Page == EImmortalManagementFeature::Crafting)
			{
				PlayerCraftingWidget->RefreshFromPlayer();
				DetailPage = PlayerCraftingWidget; ListName = TEXT("CraftingRecipeList");
			}
			if (DetailPage)
			{
				UVerticalBox* List = Cast<UVerticalBox>(DetailPage->WidgetTree->FindWidget(FName(ListName)));
				bool bSized = List && List->GetChildrenCount() > 0;
				if (List) for (UWidget* Row : List->GetAllChildren()) bSized &= Row->GetDesiredSize().Y >= 40;
				Check(TEXT("rebuilt recipe rows have height in the same frame"), bSized);
			}
			Shot(FString::Printf(TEXT("Page%d"), Index));
		});
	}
	if (bBuildings)
	{
		At(10.6f, [this] { PlayerManagementWidget->ShowScene(EImmortalManagementScene::MarketTown); });
		At(11.1f, [Shot] { Shot(TEXT("MarketBuildings")); });
	}
	At(12, [this] { ToggleAscension(); });
	At(12.6f, [this, Check, Shot] { Check(TEXT("ascension remains separate without pausing"), bAscensionOpen
		&& !bManagementInterfaceOpen && !UGameplayStatics::IsGamePaused(this)); Shot(TEXT("Ascension")); });
	At(13.5f, [this] { OpenManagementInterface(); CloseManagementInterface(); });
	At(14.5f, [this, BaseSize, Check, Shot] {
		FIntPoint Size; Cast<APlayerController>(GetController())->GetViewportSize(Size.X, Size.Y);
		Check(TEXT("closing restores compact window and combat"), Size == *BaseSize && !bManagementInterfaceOpen
			&& GetWorldTimerManager().IsTimerActive(AutoAttackTimerHandle)); Shot(TEXT("Closed"));
	});
	At(15.5f, [Failures] {
		UE_LOG(LogTemp, Display, TEXT("Desktop panels finished: failures=%d"), *Failures);
		FPlatformMisc::RequestExitWithStatus(false, *Failures ? 1 : 0);
	});
#endif
}
