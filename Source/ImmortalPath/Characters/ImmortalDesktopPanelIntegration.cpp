#include "ImmortalPlayerCharacter.h"
#if !UE_BUILD_SHIPPING
#include "../UI/ImmortalManagementWidget.h"
#include "../UI/ImmortalDesktopPanelLayout.h"
#include "../UI/ImmortalAlchemyWidget.h"
#include "../UI/ImmortalCraftingWidget.h"
#include "../UI/ImmortalShopWidget.h"
#include "../UI/ImmortalCultivationWidget.h"
#include "../UI/ImmortalAscensionWidget.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "../UI/ImmortalSectWidget.h"
#include "../UI/ImmortalFarmingWidget.h"
#include "../UI/ImmortalInventoryWidget.h"
#include "../UI/ImmortalInventorySlotWidget.h"
#include "../UI/ImmortalCraftingArt.h"
#include "../UI/ImmortalIconWidget.h"
#include "../UI/ImmortalMaterialDropWidget.h"
#include "../Drops/ImmortalMaterialDrop.h"
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
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "UObject/UnrealType.h"

namespace
{
	bool IsItemArtVisible(const UWidget* Widget)
	{
		return Widget && Widget->GetVisibility() != ESlateVisibility::Hidden
			&& Widget->GetVisibility() != ESlateVisibility::Collapsed;
	}

	bool SameItemArt(const FSlateBrush& A, const FSlateBrush& B)
	{
		const FBox2f AUV(A.GetUVRegion()), BUV(B.GetUVRegion());
		return A.DrawAs == ESlateBrushDrawType::Image && B.DrawAs == ESlateBrushDrawType::Image
			&& A.GetResourceObject() && A.GetResourceObject() == B.GetResourceObject()
			&& AUV.bIsValid && BUV.bIsValid && AUV.Min.Equals(BUV.Min) && AUV.Max.Equals(BUV.Max);
	}

	FString ItemArtSignature(const FSlateBrush& Brush)
	{
		const FBox2f UV(Brush.GetUVRegion());
		return FString::Printf(TEXT("%s:%.5f,%.5f,%.5f,%.5f"), *GetPathNameSafe(Brush.GetResourceObject()),
			UV.Min.X, UV.Min.Y, UV.Max.X, UV.Max.Y);
	}

	UImmortalInventorySlotWidget* FindItemArtCell(UUniformGridPanel* Grid, const FString& Tooltip)
	{
		if (Grid) for (UWidget* Child : Grid->GetAllChildren())
		{
			auto* Cell = Cast<UImmortalInventorySlotWidget>(Child);
			const UButton* Button = Cell && Cell->WidgetTree
				? Cast<UButton>(Cell->WidgetTree->FindWidget(TEXT("InventorySlotButton"))) : nullptr;
			if (Button && Button->GetToolTipText().ToString() == Tooltip) return Cell;
		}
		return nullptr;
	}
}
#endif

void AImmortalPlayerCharacter::RunDesktopPanelFixture()
{
#if !UE_BUILD_SHIPPING
	const bool bBuildings = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestDesktopBuildings"));
	const bool bItemArt = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestItemArt"));
	const bool bInventoryPreview = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestInventoryPreview"));
	const bool bInventoryFixture = bInventoryPreview || FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestInventoryPanels"));
	const bool bLayoutPreview = bInventoryPreview || FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestFeatureLayoutPreview"));
	if (!bBuildings && !bItemArt && !bLayoutPreview && !bInventoryFixture && !FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestDesktopPanels"))) return;
	FString UserDir;
	if (!FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir)
		|| !FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(UserDir),
			FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved/Automation/DesktopPanels")))))
	{
		UE_LOG(LogTemp, Error, TEXT("Desktop panel fixture refused: isolated UserDir required.")); return;
	}
	if (bItemArt)
	{
		// Deterministic, real player-owned pages; all mutations and saves stay in the isolated UserDir.
		if (!FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestDisableAutoBattle")))
		{
			UE_LOG(LogTemp, Error, TEXT("R06 item art fixture requires ImmortalTestDisableAutoBattle."));
			FPlatformMisc::RequestExitWithStatus(false, 1);
			return;
		}
		const auto Failures = MakeShared<int32>(0), Checks = MakeShared<int32>(0);
		const auto Check = [Failures, Checks](const FString& Name, const bool bPassed)
		{
			++*Checks;
			if (!bPassed) ++*Failures;
			UE_LOG(LogTemp, Display, TEXT("R06 item art check: %s passed=%s"), *Name,
				bPassed ? TEXT("true") : TEXT("false"));
		};
		const auto At = [this](const float Time, TFunction<void()> Work)
		{
			FTimerHandle Timer;
			GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [Work] { Work(); }), Time, false);
		};
		const auto Shot = [UserDir](const FString& Name)
		{
			IFileManager::Get().MakeDirectory(*FPaths::Combine(UserDir, TEXT("Screenshots")), true);
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(UserDir, TEXT("Screenshots"),
				TEXT("R06_") + Name + TEXT(".png")), true, false);
		};
		UTexture2D* EquipmentAtlas = LoadObject<UTexture2D>(nullptr, TEXT("/Game/GAME/Asset/DesktopPixelV2/UI/T_EquipmentAtlas.T_EquipmentAtlas"));
		UTexture2D* ForgeAtlas = LoadObject<UTexture2D>(nullptr, TEXT("/Game/GAME/Asset/DesktopPixelV2/UI/T_ForgeAtlas.T_ForgeAtlas"));
		UTexture2D* MaterialAtlas = LoadObject<UTexture2D>(nullptr, TEXT("/Game/GAME/Asset/DesktopPixelV2/UI/T_MaterialAtlas.T_MaterialAtlas"));
		UTexture2D* AlchemyAtlas = LoadObject<UTexture2D>(nullptr, TEXT("/Game/GAME/Asset/DesktopPixelV2/UI/T_AlchemyAtlas.T_AlchemyAtlas"));
		Check(TEXT("all three item families and forge atlas load"), EquipmentAtlas && ForgeAtlas && MaterialAtlas && AlchemyAtlas);
		const TArray<FName> Materials = {TEXT("SpiritGrass"), TEXT("DemonCore"), TEXT("SpiritLiquid"), TEXT("Ore"),
			TEXT("DemonBone"), TEXT("ArtifactFragment"), TEXT("SpiritIron"), TEXT("ImmortalFruit"), TEXT("SpiritWood")};
		const TArray<FName> Pills = {TEXT("HealingPill"), TEXT("QiGatheringPill"), TEXT("FoundationPill"),
			TEXT("EnlightenmentPill"), TEXT("BreakthroughPill")};
		const auto CheckCell = [Check](UImmortalInventorySlotWidget* Cell, const FSlateBrush& Expected, const FString& Name)
		{
			const UImage* Icon = Cell && Cell->WidgetTree ? Cast<UImage>(Cell->WidgetTree->FindWidget(TEXT("InventoryItemIcon"))) : nullptr;
			const UTextBlock* Glyph = Cell && Cell->WidgetTree ? Cast<UTextBlock>(Cell->WidgetTree->FindWidget(TEXT("InventoryMaterialGlyph"))) : nullptr;
			Check(Name, IsItemArtVisible(Icon) && SameItemArt(Icon->GetBrush(), Expected)
				&& Glyph && Glyph->GetVisibility() == ESlateVisibility::Collapsed);
		};
		const auto CheckDetail = [this, Check](UImmortalInventorySlotWidget* Cell, const FString& Name)
		{
			auto* Preview = PlayerInventoryWidget && PlayerInventoryWidget->WidgetTree
				? Cast<UImmortalInventorySlotWidget>(PlayerInventoryWidget->WidgetTree->FindWidget(TEXT("SelectedItemCell"))) : nullptr;
			const UImage* Icon = Cell && Cell->WidgetTree ? Cast<UImage>(Cell->WidgetTree->FindWidget(TEXT("InventoryItemIcon"))) : nullptr;
			const UImage* Detail = Preview && Preview->WidgetTree ? Cast<UImage>(Preview->WidgetTree->FindWidget(TEXT("InventoryItemIcon"))) : nullptr;
			Check(Name, IsItemArtVisible(Preview) && IsItemArtVisible(Icon) && IsItemArtVisible(Detail)
				&& SameItemArt(Icon->GetBrush(), Detail->GetBrush()));
		};
		At(0.8f, [this, Materials, Pills]
		{
			InvulnerableUntilTime = GetWorld()->GetTimeSeconds() + 120;
			bAutoEquipNewItems = false;
			InventoryItems.Reset(); EquippedItems.Reset(); MaterialInventory.Reset(); PillInventory.Reset();
			ArtifactInventory.Reset(); QuestItemInventory.Reset(); EquippedArtifactInstanceId.Invalidate();
			for (int32 Index = 0; Index < 9; ++Index)
			{
				auto Item = UImmortalEquipmentLibrary::GenerateCraftedEquipment(20 + Index,
					static_cast<EImmortalEquipmentSlot>(Index), static_cast<EImmortalEquipmentQuality>(Index % 7));
				Item.bLocked = Index % 2 == 0;
				InventoryItems.Add(Item);
			}
			for (FName Id : Materials) MaterialInventory.Add(FImmortalMaterialStack{Id, 31});
			for (FName Id : Pills) for (int32 Quality = 0; Quality < 2; ++Quality)
			{
				FImmortalPillStack Stack; Stack.PillId = Id; Stack.Quality = static_cast<EImmortalPillQuality>(Quality); Stack.Quantity = 23;
				PillInventory.Add(Stack);
			}
			for (FName Id : UImmortalArtifactLibrary::GetKnownArtifactIds())
				ArtifactInventory.Add(UImmortalArtifactLibrary::CreateArtifact(Id));
			for (FName Id : UImmortalInventoryLibrary::GetKnownQuestItemIds()) QuestItemInventory.Add(FImmortalQuestItemStack{Id, 17});
			++EquipmentInventoryRevision; ++MaterialInventoryRevision; ++PillInventoryRevision;
			++ArtifactInventoryRevision; ++QuestItemInventoryRevision;
		});
		At(1.6f, [this] { OpenManagementFeature(EImmortalManagementFeature::Inventory); });
		At(2.2f, [this, Check, CheckCell, CheckDetail, EquipmentAtlas]
		{
			if (!PlayerInventoryWidget || !PlayerInventoryWidget->WidgetTree) { Check(TEXT("player inventory exists"), false); return; }
			auto* Grid = Cast<UUniformGridPanel>(PlayerInventoryWidget->WidgetTree->FindWidget(TEXT("BackpackGrid")));
			Check(TEXT("nine equipment rows exist"), Grid && Grid->GetChildrenCount() >= 9 && InventoryItems.Num() == 9);
			TSet<FString> DistinctArt;
			for (int32 Index = 0; Grid && Index < 9 && InventoryItems.IsValidIndex(Index); ++Index)
			{
				const auto Item = InventoryItems[Index];
				auto* Cell = Cast<UImmortalInventorySlotWidget>(Grid->GetChildAt(Index));
				const FSlateBrush Expected = ImmortalCraftingArt::EquipmentBrush(EquipmentAtlas, Item.Slot);
				CheckCell(Cell, Expected, FString::Printf(TEXT("equipment %d shared atlas without fallback overlap"), Index));
				const UImage* Icon = Cell ? Cast<UImage>(Cell->WidgetTree->FindWidget(TEXT("InventoryItemIcon"))) : nullptr;
				if (Icon) DistinctArt.Add(ItemArtSignature(Icon->GetBrush()));
				const UImage* Frame = Cell ? Cast<UImage>(Cell->WidgetTree->FindWidget(TEXT("InventoryQualityFrame"))) : nullptr;
				const UTextBlock* Lock = Cell ? Cast<UTextBlock>(Cell->WidgetTree->FindWidget(TEXT("InventoryLockGlyph"))) : nullptr;
				const UTextBlock* Level = Cell ? Cast<UTextBlock>(Cell->WidgetTree->FindWidget(TEXT("InventoryItemLevel"))) : nullptr;
				Check(FString::Printf(TEXT("equipment %d quality lock and level"), Index), IsItemArtVisible(Frame)
					&& Frame->GetBrush().OutlineSettings.Color.GetSpecifiedColor().Equals(UImmortalEquipmentLibrary::GetQualityColor(Item.Quality))
					&& Lock && IsItemArtVisible(Lock) == Item.bLocked && IsItemArtVisible(Level)
					&& Level->GetText().ToString() == FText::AsNumber(Item.ItemLevel).ToString());
				const UTextBlock* Label = Cell ? Cast<UTextBlock>(Cell->WidgetTree->FindWidget(TEXT("InventorySlotLabel"))) : nullptr;
				const TCHAR* ExpectedLabel = Item.Slot == EImmortalEquipmentSlot::Bracers ? TEXT("腕")
					: Item.Slot == EImmortalEquipmentSlot::Belt ? TEXT("带") : Item.Slot == EImmortalEquipmentSlot::RingLeft ? TEXT("戒1")
					: Item.Slot == EImmortalEquipmentSlot::RingRight ? TEXT("戒2") : TEXT("");
				Check(FString::Printf(TEXT("equipment %d slot badge"), Index), Label && (FCString::Strlen(ExpectedLabel)
					? IsItemArtVisible(Label) && Label->GetText().ToString() == ExpectedLabel : !IsItemArtVisible(Label)));
				PlayerInventoryWidget->HandleSlotSelected(Item.ItemId);
				// Selection rebuilds the grid, so resolve the current cell before comparing the detail preview.
				Grid = Cast<UUniformGridPanel>(PlayerInventoryWidget->WidgetTree->FindWidget(TEXT("BackpackGrid")));
				CheckDetail(Cast<UImmortalInventorySlotWidget>(Grid->GetChildAt(Index)), FString::Printf(TEXT("equipment %d detail matches grid"), Index));
			}
			Check(TEXT("all nine equipment UVs are distinct"), DistinctArt.Num() == 9);
		});
		At(2.7f, [Shot] { Shot(TEXT("Equipment")); });
		At(3.2f, [this] { if (PlayerInventoryWidget) PlayerInventoryWidget->ShowMaterialTab(); });
		At(3.8f, [this, Materials, ForgeAtlas, MaterialAtlas, Check, CheckCell, CheckDetail]
		{
			if (!PlayerInventoryWidget) return;
			TSet<FString> DistinctArt;
			for (FName Id : Materials)
			{
				FImmortalMaterialDefinition Definition; UImmortalMaterialLibrary::GetMaterialDefinition(Id, Definition);
				PlayerInventoryWidget->HandleMaterialSelected(Id);
				auto* Grid = Cast<UUniformGridPanel>(PlayerInventoryWidget->WidgetTree->FindWidget(TEXT("BackpackGrid")));
				auto* Cell = FindItemArtCell(Grid, Definition.DisplayName.ToString());
				const FSlateBrush Expected = ImmortalCraftingArt::MaterialBrush(ForgeAtlas, MaterialAtlas, Id);
				CheckCell(Cell, Expected, Id.ToString() + TEXT(" material atlas without fallback overlap"));
				CheckDetail(Cell, Id.ToString() + TEXT(" material detail matches grid"));
				if (Cell) DistinctArt.Add(ItemArtSignature(CastChecked<UImage>(Cell->WidgetTree->FindWidget(TEXT("InventoryItemIcon")))->GetBrush()));
				const UTextBlock* Quantity = Cell ? Cast<UTextBlock>(Cell->WidgetTree->FindWidget(TEXT("InventoryItemLevel"))) : nullptr;
				Check(Id.ToString() + TEXT(" material quantity"), IsItemArtVisible(Quantity) && Quantity->GetText().ToString() == TEXT("×31"));
			}
			Check(TEXT("nine material illustrations are distinct"), DistinctArt.Num() == 9);
		});
		At(4.4f, [Shot] { Shot(TEXT("Materials")); });
		At(5.0f, [this] { if (PlayerInventoryWidget) PlayerInventoryWidget->ShowPillTab(); });
		At(5.6f, [this, Pills, AlchemyAtlas, Check, CheckCell, CheckDetail]
		{
			if (!PlayerInventoryWidget) return;
			TSet<FString> DistinctArt;
			for (FName Id : Pills) for (int32 Quality = 0; Quality < 2; ++Quality)
			{
				const auto PillQuality = static_cast<EImmortalPillQuality>(Quality);
				FImmortalPillDefinition Definition; UImmortalAlchemyLibrary::GetPillDefinition(Id, Definition);
				PlayerInventoryWidget->HandlePillSelected(Id, PillQuality);
				auto* Grid = Cast<UUniformGridPanel>(PlayerInventoryWidget->WidgetTree->FindWidget(TEXT("BackpackGrid")));
				auto* Cell = FindItemArtCell(Grid, Definition.DisplayName.ToString() + TEXT(" · ") + UImmortalAlchemyLibrary::GetQualityText(PillQuality).ToString());
				CheckCell(Cell, ImmortalAlchemyArt::Brush(AlchemyAtlas, Id), Id.ToString() + TEXT(" pill atlas without fallback overlap"));
				CheckDetail(Cell, Id.ToString() + TEXT(" pill detail matches grid"));
				const UImage* Icon = Cell ? Cast<UImage>(Cell->WidgetTree->FindWidget(TEXT("InventoryItemIcon"))) : nullptr;
				if (Icon) DistinctArt.Add(ItemArtSignature(Icon->GetBrush()));
				const UImage* Frame = Cell ? Cast<UImage>(Cell->WidgetTree->FindWidget(TEXT("InventoryQualityFrame"))) : nullptr;
				const UTextBlock* Quantity = Cell ? Cast<UTextBlock>(Cell->WidgetTree->FindWidget(TEXT("InventoryItemLevel"))) : nullptr;
				Check(Id.ToString() + TEXT(" pill quality and quantity"), IsItemArtVisible(Frame)
					&& Frame->GetBrush().OutlineSettings.Color.GetSpecifiedColor().Equals(UImmortalAlchemyLibrary::GetQualityColor(PillQuality))
					&& IsItemArtVisible(Quantity) && Quantity->GetText().ToString() == TEXT("×23"));
			}
			Check(TEXT("five pill illustrations remain distinct across both qualities"), DistinctArt.Num() == 5);
		});
		At(6.2f, [Shot] { Shot(TEXT("Pills")); });
		for (int32 Kind = 0; Kind < 2; ++Kind)
		{
			At(7.0f + Kind * 1.4f, [this, Kind, Check]
			{
				if (!PlayerInventoryWidget) return;
				if (Kind == 0) PlayerInventoryWidget->ShowArtifactTab(); else PlayerInventoryWidget->ShowQuestItemTab();
				auto* Grid = Cast<UUniformGridPanel>(PlayerInventoryWidget->WidgetTree->FindWidget(TEXT("BackpackGrid")));
				const int32 Count = Kind == 0 ? ArtifactInventory.Num() : QuestItemInventory.Num();
				Check(Kind == 0 ? TEXT("artifact fallback catalog is populated") : TEXT("quest fallback catalog is populated"), Grid && Count > 0);
				for (int32 Index = 0; Grid && Index < Count; ++Index)
				{
					if (Kind == 0) PlayerInventoryWidget->HandleArtifactSelected(ArtifactInventory[Index].InstanceId);
					else PlayerInventoryWidget->HandleQuestItemSelected(QuestItemInventory[Index].QuestItemId);
					Grid = Cast<UUniformGridPanel>(PlayerInventoryWidget->WidgetTree->FindWidget(TEXT("BackpackGrid")));
					auto* Cell = Cast<UImmortalInventorySlotWidget>(Grid->GetChildAt(Index));
					auto* Preview = Cast<UImmortalInventorySlotWidget>(PlayerInventoryWidget->WidgetTree->FindWidget(TEXT("SelectedItemCell")));
					FText ExpectedGlyph;
					if (Kind == 0)
					{
						FImmortalArtifactDefinition Definition;
						UImmortalArtifactLibrary::GetArtifactDefinition(ArtifactInventory[Index].ArtifactId, Definition);
						ExpectedGlyph = Definition.IconGlyph.IsEmpty() ? FText::FromString(TEXT("宝")) : Definition.IconGlyph;
					}
					else
					{
						FImmortalQuestItemDefinition Definition;
						UImmortalInventoryLibrary::GetQuestItemDefinition(QuestItemInventory[Index].QuestItemId, Definition);
						ExpectedGlyph = Definition.IconGlyph.IsEmpty() ? FText::FromString(TEXT("任")) : Definition.IconGlyph;
					}
					bool bExplicitFallback = IsItemArtVisible(Preview);
					for (auto* VisibleCell : {Cell, Preview})
					{
						const UImage* Icon = VisibleCell ? Cast<UImage>(VisibleCell->WidgetTree->FindWidget(TEXT("InventoryItemIcon"))) : nullptr;
						const UTextBlock* Glyph = VisibleCell ? Cast<UTextBlock>(VisibleCell->WidgetTree->FindWidget(TEXT("InventoryMaterialGlyph"))) : nullptr;
						bExplicitFallback &= Icon && !IsItemArtVisible(Icon) && IsItemArtVisible(Glyph)
							&& Glyph->GetParent() && Glyph->GetText().ToString() == ExpectedGlyph.ToString();
					}
					Check(FString::Printf(TEXT("%s %d grid and detail retain explicit glyph fallback"), Kind == 0 ? TEXT("artifact") : TEXT("quest"), Index), bExplicitFallback);
				}
			});
			At(7.7f + Kind * 1.4f, [Shot, Kind] { Shot(Kind == 0 ? TEXT("ArtifactsFallback") : TEXT("QuestFallback")); });
		}
		At(10.0f, [this] { OpenManagementFeature(EImmortalManagementFeature::Alchemy); });
		At(10.6f, [this, Check, AlchemyAtlas]
		{
			auto* Grid = PlayerAlchemyWidget ? Cast<UUniformGridPanel>(PlayerAlchemyWidget->WidgetTree->FindWidget(TEXT("PillGrid"))) : nullptr;
			Check(TEXT("alchemy owns ten real pill cells"), Grid && Grid->GetChildrenCount() == PillInventory.Num() && PillInventory.Num() == 10);
			for (int32 Index = 0; Grid && Index < PillInventory.Num(); ++Index)
			{
				auto* Cell = Cast<UUserWidget>(Grid->GetChildAt(Index));
				const UImage* Icon = Cell ? Cast<UImage>(Cell->WidgetTree->FindWidget(TEXT("PillArt"))) : nullptr;
				const UTextBlock* Glyph = Cell ? Cast<UTextBlock>(Cell->WidgetTree->FindWidget(TEXT("PillGlyph"))) : nullptr;
				Check(FString::Printf(TEXT("alchemy pill %d uses the same catalog atlas without glyph overlap"), Index), IsItemArtVisible(Icon)
					&& SameItemArt(Icon->GetBrush(), ImmortalAlchemyArt::Brush(AlchemyAtlas, PillInventory[Index].PillId))
					&& Glyph && !IsItemArtVisible(Glyph));
			}
		});
		At(11.2f, [Shot] { Shot(TEXT("Alchemy")); });
		At(12.0f, [this, Materials, Pills]
		{
			ShopState.Listings.Reset();
			ShopState.RefreshDayKey = UImmortalShopLibrary::GetDayKeyFromUtcTicks(FDateTime::UtcNow().GetTicks());
			for (const auto& Item : InventoryItems)
			{
				FImmortalShopListing Listing; Listing.ListingId = FGuid::NewGuid(); Listing.ProductType = EImmortalShopProductType::Equipment;
				Listing.EquipmentItem = Item; Listing.BundlePrice = 100; ShopState.Listings.Add(Listing);
			}
			for (FName Id : Materials)
			{
				FImmortalShopListing Listing; Listing.ListingId = FGuid::NewGuid(); Listing.ProductType = EImmortalShopProductType::Material;
				Listing.ProductId = Id; Listing.BundleQuantity = 31; Listing.BundlePrice = 100; ShopState.Listings.Add(Listing);
			}
			for (FName Id : Pills)
			{
				FImmortalShopListing Listing; Listing.ListingId = FGuid::NewGuid(); Listing.ProductType = EImmortalShopProductType::Pill;
				Listing.ProductId = Id; Listing.BundlePrice = 100; ShopState.Listings.Add(Listing);
			}
			for (const auto& Item : ArtifactInventory)
			{
				FImmortalShopListing Listing; Listing.ListingId = FGuid::NewGuid(); Listing.ProductType = EImmortalShopProductType::Artifact;
				Listing.ProductId = Item.ArtifactId; Listing.BundlePrice = 100; ShopState.Listings.Add(Listing);
			}
			++ShopRevision;
			OpenManagementFeature(EImmortalManagementFeature::Shop);
		});
		At(12.6f, [this, Check, EquipmentAtlas, ForgeAtlas, MaterialAtlas, AlchemyAtlas]
		{
			if (!PlayerShopWidget) { Check(TEXT("player shop exists"), false); return; }
			auto* List = Cast<UVerticalBox>(PlayerShopWidget->WidgetTree->FindWidget(TEXT("ShopOfferList")));
			Check(TEXT("shop includes all equipment material pill and artifact offers"), List && List->GetChildrenCount() == ShopState.Listings.Num()
				&& ShopState.Listings.Num() == 27);
			for (int32 Index = 0; List && Index < ShopState.Listings.Num(); ++Index)
			{
				const auto Listing = ShopState.Listings[Index];
				PlayerShopWidget->SelectOffer(Listing.ListingId);
				// Selection rebuilds the list; inspect the currently displayed row, not a removed widget.
				List = Cast<UVerticalBox>(PlayerShopWidget->WidgetTree->FindWidget(TEXT("ShopOfferList")));
				auto* Row = List ? Cast<UUserWidget>(List->GetChildAt(Index)) : nullptr;
				const UImage* Art = Row ? Cast<UImage>(Row->WidgetTree->FindWidget(TEXT("ShopEntryArt"))) : nullptr;
				UImmortalIconWidget* Fallback = nullptr;
				if (Row) Row->WidgetTree->ForEachWidget([&](UWidget* Widget) { if (auto* Icon = Cast<UImmortalIconWidget>(Widget)) Fallback = Icon; });
				const UImage* Detail = Cast<UImage>(PlayerShopWidget->WidgetTree->FindWidget(TEXT("ShopOfferIcon")));
				const UWidget* DetailFallback = PlayerShopWidget->WidgetTree->FindWidget(TEXT("ShopOfferFallback"));
				const FSlateBrush Expected = Listing.ProductType == EImmortalShopProductType::Equipment
					? ImmortalCraftingArt::EquipmentBrush(EquipmentAtlas, Listing.EquipmentItem.Slot)
					: Listing.ProductType == EImmortalShopProductType::Material
						? ImmortalCraftingArt::MaterialBrush(ForgeAtlas, MaterialAtlas, Listing.ProductId)
						: Listing.ProductType == EImmortalShopProductType::Pill
							? ImmortalAlchemyArt::Brush(AlchemyAtlas, Listing.ProductId) : FSlateBrush();
				const bool bAtlas = Expected.DrawAs == ESlateBrushDrawType::Image && Expected.GetResourceObject();
				Check(FString::Printf(TEXT("shop offer %d list and detail agree on art or explicit fallback"), Index), Art && Detail && Fallback && DetailFallback
					&& (bAtlas ? SameItemArt(Art->GetBrush(), Expected) && SameItemArt(Detail->GetBrush(), Expected)
						&& !IsItemArtVisible(Fallback) && !IsItemArtVisible(DetailFallback)
						: Art->GetBrush().DrawAs == ESlateBrushDrawType::NoDrawType && Detail->GetBrush().DrawAs == ESlateBrushDrawType::NoDrawType
						&& IsItemArtVisible(Fallback) && IsItemArtVisible(DetailFallback)));
			}
			for (const auto& Item : InventoryItems)
			{
				PlayerShopWidget->SelectEquipmentForSale(Item.ItemId);
				const UImage* Detail = Cast<UImage>(PlayerShopWidget->WidgetTree->FindWidget(TEXT("ShopSaleIcon")));
				Check(FString::Printf(TEXT("shop equipment sale %d shares inventory catalog"), static_cast<int32>(Item.Slot)), IsItemArtVisible(Detail)
					&& SameItemArt(Detail->GetBrush(), ImmortalCraftingArt::EquipmentBrush(EquipmentAtlas, Item.Slot)));
			}
			for (const auto& Stack : MaterialInventory)
			{
				PlayerShopWidget->SelectMaterialForSale(Stack.MaterialId);
				const UImage* Detail = Cast<UImage>(PlayerShopWidget->WidgetTree->FindWidget(TEXT("ShopSaleIcon")));
				Check(Stack.MaterialId.ToString() + TEXT(" shop material sale shares inventory catalog"), IsItemArtVisible(Detail)
					&& SameItemArt(Detail->GetBrush(), ImmortalCraftingArt::MaterialBrush(ForgeAtlas, MaterialAtlas, Stack.MaterialId)));
			}
			if (!ShopState.Listings.IsEmpty()) PlayerShopWidget->SelectOffer(ShopState.Listings[0].ListingId);
		});
		At(13.2f, [Shot] { Shot(TEXT("ShopEquipment")); });
		for (int32 Kind = 0; Kind < 3; ++Kind)
		{
			At(14.0f + Kind * 1.4f, [this, Kind, Check]
			{
				if (!PlayerShopWidget) return;
				const auto ProductType = Kind == 0 ? EImmortalShopProductType::Material
					: Kind == 1 ? EImmortalShopProductType::Pill : EImmortalShopProductType::Artifact;
				const auto* Listing = ShopState.Listings.FindByPredicate([ProductType](const auto& Entry) { return Entry.ProductType == ProductType; });
				if (Listing) PlayerShopWidget->SelectOffer(Listing->ListingId);
				Check(TEXT("shop category remains inspectable"), Listing && !UGameplayStatics::IsGamePaused(this));
				if (Kind == 0 && !MaterialInventory.IsEmpty())
				{
					PlayerShopWidget->SelectMaterialForSale(MaterialInventory[0].MaterialId);
					const UImage* Art = Cast<UImage>(PlayerShopWidget->WidgetTree->FindWidget(TEXT("ShopSaleIcon")));
					Check(TEXT("shop sale material detail uses shared catalog"), IsItemArtVisible(Art)
						&& SameItemArt(Art->GetBrush(), PlayerShopWidget->GetMaterialArt(MaterialInventory[0].MaterialId)));
				}
			});
			At(14.7f + Kind * 1.4f, [Shot, Kind] { Shot(Kind == 0 ? TEXT("ShopMaterial") : Kind == 1 ? TEXT("ShopPill") : TEXT("ShopArtifactFallback")); });
		}
		At(18.0f, [this, Check]
		{
			if (!PlayerShopWidget) { Check(TEXT("shop missing-art fixture has a real page"), false); return; }
			// Exercise missing assets on this isolated widget instance, never on a class default.
			for (const FName Name : {FName(TEXT("EquipmentAtlas")), FName(TEXT("ForgeAtlas")),
				FName(TEXT("MaterialAtlas")), FName(TEXT("AlchemyAtlas"))})
			{
				auto* Property = FindFProperty<FSoftObjectProperty>(PlayerShopWidget->GetClass(), Name);
				Check(Name.ToString() + TEXT(" test instance property exists"), Property != nullptr);
				if (Property) Property->SetPropertyValue_InContainer(PlayerShopWidget, FSoftObjectPtr());
			}
			for (const auto Type : {EImmortalShopProductType::Equipment, EImmortalShopProductType::Material, EImmortalShopProductType::Pill})
			{
				const int32 Index = ShopState.Listings.IndexOfByPredicate([Type](const auto& Entry) { return Entry.ProductType == Type; });
				if (Index == INDEX_NONE) { Check(TEXT("missing-art category has a listing"), false); continue; }
				PlayerShopWidget->SelectOffer(ShopState.Listings[Index].ListingId);
				auto* List = Cast<UVerticalBox>(PlayerShopWidget->WidgetTree->FindWidget(TEXT("ShopOfferList")));
				auto* Row = List ? Cast<UUserWidget>(List->GetChildAt(Index)) : nullptr;
				const UImage* Art = Row ? Cast<UImage>(Row->WidgetTree->FindWidget(TEXT("ShopEntryArt"))) : nullptr;
				UImmortalIconWidget* Fallback = nullptr;
				if (Row) Row->WidgetTree->ForEachWidget([&](UWidget* Widget) { if (auto* Icon = Cast<UImmortalIconWidget>(Widget)) Fallback = Icon; });
				const UImage* Detail = Cast<UImage>(PlayerShopWidget->WidgetTree->FindWidget(TEXT("ShopOfferIcon")));
				Check(FString::Printf(TEXT("shop missing-art category %d has live row and detail fallback"), static_cast<int32>(Type)),
					Art && !Art->GetBrush().GetResourceObject() && IsItemArtVisible(Fallback)
					&& Detail && !Detail->GetBrush().GetResourceObject()
					&& IsItemArtVisible(PlayerShopWidget->WidgetTree->FindWidget(TEXT("ShopOfferFallback"))));
			}
			if (!InventoryItems.IsEmpty()) PlayerShopWidget->SelectEquipmentForSale(InventoryItems[0].ItemId);
			Check(TEXT("shop missing equipment sale art has explicit fallback"),
				!IsItemArtVisible(PlayerShopWidget->WidgetTree->FindWidget(TEXT("ShopSaleIcon")))
				&& IsItemArtVisible(PlayerShopWidget->WidgetTree->FindWidget(TEXT("ShopSaleFallback"))));
			if (!MaterialInventory.IsEmpty()) PlayerShopWidget->SelectMaterialForSale(MaterialInventory[0].MaterialId);
			Check(TEXT("shop missing material sale art has explicit fallback"),
				!IsItemArtVisible(PlayerShopWidget->WidgetTree->FindWidget(TEXT("ShopSaleIcon")))
				&& IsItemArtVisible(PlayerShopWidget->WidgetTree->FindWidget(TEXT("ShopSaleFallback"))));
		});
		At(18.2f, [Shot] { Shot(TEXT("ShopMissingArtFallback")); });
		const auto Drops = MakeShared<TArray<TWeakObjectPtr<AImmortalMaterialDrop>>>();
		At(18.6f, [this, Materials, Drops]
		{
			CloseManagementInterface();
			for (int32 Index = 0; Index < Materials.Num(); ++Index)
			{
				const FVector Location = GetActorLocation() + FVector(180 + Index * 130, 0, 95);
				auto* Drop = GetWorld()->SpawnActor<AImmortalMaterialDrop>(AImmortalMaterialDrop::StaticClass(), Location, FRotator::ZeroRotator);
				if (!Drop) continue;
				Drop->SetActorTickEnabled(false);
				if (auto* Visual = Drop->FindComponentByClass<UWidgetComponent>()) Visual->InitWidget();
				Drop->SetMaterialDrop(Materials[Index], 31);
				Drops->Add(Drop);
			}
		});
		At(19.2f, [Materials, Drops, ForgeAtlas, MaterialAtlas, Check]
		{
			Check(TEXT("all nine real material actors spawned"), Drops->Num() == Materials.Num());
			for (const auto& Entry : *Drops)
			{
				const auto* Drop = Entry.Get();
				const auto* Visual = Drop ? Drop->FindComponentByClass<UWidgetComponent>() : nullptr;
				const auto* Widget = Visual ? Cast<UImmortalMaterialDropWidget>(Visual->GetUserWidgetObject()) : nullptr;
				const UImage* Icon = Widget ? Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("MaterialIcon"))) : nullptr;
				const UTextBlock* Glyph = Widget ? Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("MaterialGlyph"))) : nullptr;
				Check(Drop ? Drop->GetMaterialId().ToString() + TEXT(" real drop shares inventory catalog without glyph overlap") : TEXT("material drop exists"),
					Drop && Drop->GetQuantity() == 31 && IsItemArtVisible(Icon) && Glyph && !IsItemArtVisible(Glyph)
					&& SameItemArt(Icon->GetBrush(), ImmortalCraftingArt::MaterialBrush(ForgeAtlas, MaterialAtlas, Drop->GetMaterialId())));
			}
		});
		At(19.8f, [Shot] { Shot(TEXT("MaterialDrops")); });
		At(20.6f, [this, Check, Drops]
		{
			for (const auto& Drop : *Drops) if (Drop.IsValid()) Drop->Destroy();
			OpenManagementFeature(EImmortalManagementFeature::Inventory);
			InventoryItems.Reset(); EquippedItems.Reset(); MaterialInventory.Reset(); PillInventory.Reset(); ArtifactInventory.Reset(); QuestItemInventory.Reset();
			++EquipmentInventoryRevision; ++MaterialInventoryRevision; ++PillInventoryRevision; ++ArtifactInventoryRevision; ++QuestItemInventoryRevision;
			if (!PlayerInventoryWidget) return;
			for (const auto Category : {EImmortalInventoryCategory::Equipment, EImmortalInventoryCategory::Material,
				EImmortalInventoryCategory::Pill, EImmortalInventoryCategory::Artifact, EImmortalInventoryCategory::QuestItem})
			{
				PlayerInventoryWidget->ShowCategory(Category);
				auto* Grid = Cast<UUniformGridPanel>(PlayerInventoryWidget->WidgetTree->FindWidget(TEXT("BackpackGrid")));
				bool bNoStaleArt = Grid != nullptr;
				if (Grid) for (auto* Child : Grid->GetAllChildren())
				{
					auto* Cell = Cast<UImmortalInventorySlotWidget>(Child);
					for (const TCHAR* Name : {TEXT("InventoryItemIcon"), TEXT("InventoryQualityFrame"), TEXT("InventoryItemLevel"), TEXT("InventoryLockGlyph")})
						bNoStaleArt &= Cell && !IsItemArtVisible(Cell->WidgetTree->FindWidget(Name));
				}
				const UWidget* Detail = PlayerInventoryWidget->WidgetTree->FindWidget(TEXT("SelectedItemCell"));
				Check(FString::Printf(TEXT("empty category %d clears actual art badges and detail"), static_cast<int32>(Category)), bNoStaleArt && Detail && !IsItemArtVisible(Detail));
			}
		});
		At(21.3f, [Shot] { Shot(TEXT("EmptyCategory")); });
		At(22.2f, [Failures, Checks]
		{
			UE_LOG(LogTemp, Display, TEXT("R06 item art finished: checks=%d failures=%d RESULT: %s"), *Checks, *Failures,
				*Failures ? TEXT("FAIL") : TEXT("PASS"));
			FPlatformMisc::RequestExitWithStatus(false, *Failures ? 1 : 0);
		});
		return;
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
			if (Page == EImmortalManagementFeature::Crafting)
			{
				UWidgetTree* Tree = PlayerCraftingWidget->WidgetTree;
				const USizeBox* Root = Cast<USizeBox>(Tree->RootWidget);
				Check(TEXT("crafting uses a full 1600x600 page"), Root && Root->GetWidthOverride() == 1600 && Root->GetHeightOverride() == 600);
				bool bArt = true;
				for (const TCHAR* Name : {TEXT("CraftingRecipeIcon"), TEXT("CraftingForgeArt"), TEXT("CraftingItemIcon")})
				{
					const UImage* Image = Cast<UImage>(Tree->FindWidget(FName(Name)));
					bArt &= Image && Image->GetBrush().GetResourceObject() && Image->GetBrush().DrawAs == ESlateBrushDrawType::Image;
				}
				Check(TEXT("crafting product furnace and selected item art load"), bArt);
				const UVerticalBox* Recipes = Cast<UVerticalBox>(Tree->FindWidget(TEXT("CraftingRecipeList")));
				bool bRows = Recipes && Recipes->GetChildrenCount() == UImmortalCraftingLibrary::GetKnownRecipeIds().Num();
				if (Recipes) for (UWidget* Entry : Recipes->GetAllChildren())
				{
					const UUserWidget* Row = Cast<UUserWidget>(Entry);
					const UImage* Icon = Row ? Cast<UImage>(Row->WidgetTree->FindWidget(TEXT("CraftingEntryIcon"))) : nullptr;
					bRows &= Icon && Icon->GetBrush().GetResourceObject() && Entry->GetDesiredSize().Y >= 88;
				}
				Check(TEXT("crafting recipes have large illustrated rows"), bRows);
				bool bBounded = true, bReadable = true;
				Tree->ForEachWidget([&](UWidget* Widget)
				{
					if (const UTextBlock* Text = Cast<UTextBlock>(Widget)) bReadable &= Text->GetFont().Size >= 18;
					if (const UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
					{
						const auto Start = CanvasSlot->GetPosition(), End = Start + CanvasSlot->GetSize();
						const auto ParentSize = Widget->GetParent()->GetCachedGeometry().GetLocalSize();
						bBounded &= Start.X >= 0 && Start.Y >= 0 && End.X <= ParentSize.X + 1 && End.Y <= ParentSize.Y + 1;
					}
				});
				Check(TEXT("crafting text is at least 18pt and controls bounded"), bBounded && bReadable);
				const auto SavedItems = InventoryItems, SavedEquipped = EquippedItems;
				const auto SavedMaterials = MaterialInventory;
				const int32 SavedGold = CurrentGold;
				InventoryItems.Reset(); EquippedItems.Reset(); ++EquipmentInventoryRevision;
				PlayerCraftingWidget->RefreshFromPlayer();
				const UButton* Enhance = Cast<UButton>(Tree->FindWidget(TEXT("EnhanceEquipmentButton")));
				const UButton* Refine = Cast<UButton>(Tree->FindWidget(TEXT("RefineEquipmentButton")));
				const UImage* ItemArt = Cast<UImage>(Tree->FindWidget(TEXT("CraftingItemIcon")));
				Check(TEXT("empty equipment hides stale art and disables forge actions"), Enhance && Refine && ItemArt
					&& !Enhance->GetIsEnabled() && !Refine->GetIsEnabled() && ItemArt->GetBrush().DrawAs == ESlateBrushDrawType::NoDrawType);
				CurrentGold = 0; MaterialInventory.Reset(); ++MaterialInventoryRevision;
				PlayerCraftingWidget->SelectRecipe(TEXT("QingyunSword"));
				const UButton* Craft = Cast<UButton>(Tree->FindWidget(TEXT("CraftEquipmentButton")));
				Check(TEXT("missing crafting resources disables craft action"), Craft && !Craft->GetIsEnabled());
				FImmortalEquipmentItem MaxItem = UImmortalEquipmentLibrary::GenerateCraftedEquipment(1,
					EImmortalEquipmentSlot::Weapon, EImmortalEquipmentQuality::Rare);
				MaxItem.EnhancementLevel = 15;
				InventoryItems.Add(MaxItem); ++EquipmentInventoryRevision;
				PlayerCraftingWidget->SelectEquipment(MaxItem.ItemId);
				const UTextBlock* MaxLabel = Cast<UTextBlock>(Tree->FindWidget(TEXT("EnhancementCost")));
				Check(TEXT("max enhancement visibly disabled and labeled"), Enhance && !Enhance->GetIsEnabled()
					&& MaxLabel && MaxLabel->GetText().ToString().Contains(TEXT("满级")));
				const bool SavedAutoEquip = bAutoEquipNewItems;
				bAutoEquipNewItems = false;
				InventoryItems.Reset();
				for (int32 SlotIndex = 0; SlotIndex < GetInventoryCapacity(); ++SlotIndex)
				{
					auto LockedItem = UImmortalEquipmentLibrary::GenerateCraftedEquipment(1,
						EImmortalEquipmentSlot::Weapon, EImmortalEquipmentQuality::Common);
					LockedItem.bLocked = true;
					InventoryItems.Add(LockedItem);
				}
				CurrentGold = 100000;
				for (FName Id : {FName(TEXT("Ore")), FName(TEXT("DemonBone"))})
				{
					FImmortalMaterialStack Stack; Stack.MaterialId = Id; Stack.Quantity = 100;
					MaterialInventory.Add(Stack);
				}
				++EquipmentInventoryRevision; ++MaterialInventoryRevision;
				PlayerCraftingWidget->SelectRecipe(TEXT("QingyunSword"));
				const UTextBlock* CraftLabel = Cast<UTextBlock>(Tree->FindWidget(TEXT("CraftEquipmentButtonText")));
				Check(TEXT("full locked backpack shows inventory reason instead of material shortage"), Craft && !Craft->GetIsEnabled()
					&& CraftLabel && CraftLabel->GetText().ToString().Contains(TEXT("储物戒已满")));
				bAutoEquipNewItems = SavedAutoEquip;
				InventoryItems = SavedItems; EquippedItems = SavedEquipped; MaterialInventory = SavedMaterials; CurrentGold = SavedGold;
				++EquipmentInventoryRevision; ++MaterialInventoryRevision;
				PlayerCraftingWidget->RefreshFromPlayer();
			}
			if (Page == EImmortalManagementFeature::Shop)
			{
				UWidgetTree* Tree = PlayerShopWidget->WidgetTree;
				const USizeBox* Root = Cast<USizeBox>(Tree->RootWidget);
				Check(TEXT("shop uses a full 1600x600 page"), Root && Root->GetWidthOverride() == 1600 && Root->GetHeightOverride() == 600);
				const UVerticalBox* List = Cast<UVerticalBox>(Tree->FindWidget(TEXT("ShopOfferList")));
				bool bIllustrated = List && List->GetChildrenCount() == ShopState.Listings.Num();
				if (List) for (int32 RowIndex = 0; RowIndex < List->GetChildrenCount(); ++RowIndex)
				{
					const UUserWidget* Row = Cast<UUserWidget>(List->GetChildAt(RowIndex));
					const UImage* Art = Row ? Cast<UImage>(Row->WidgetTree->FindWidget(TEXT("ShopEntryArt"))) : nullptr;
					bIllustrated &= Art && Row->GetDesiredSize().Y >= 90
						&& (ShopState.Listings[RowIndex].ProductType == EImmortalShopProductType::Artifact || Art->GetBrush().GetResourceObject());
				}
				Check(TEXT("shop catalog has large illustrated equipment material and pill rows"), bIllustrated);
				const auto SavedShop = ShopState;
				const auto SavedInventory = InventoryItems;
				const auto SavedMaterials = MaterialInventory;
				const int32 SavedGold = CurrentGold;
				UButton* Buy = Cast<UButton>(Tree->FindWidget(TEXT("ShopBuyButton")));
				UButton* Sell = Cast<UButton>(Tree->FindWidget(TEXT("ShopSellOneButton")));
				UButton* SellAll = Cast<UButton>(Tree->FindWidget(TEXT("ShopSellAllButton")));
				const UTextBlock* Result = Cast<UTextBlock>(Tree->FindWidget(TEXT("ShopResult")));
				CurrentGold = 0;
				if (!ShopState.Listings.IsEmpty())
				{
					ShopState.Listings[0].bSoldOut = false;
					ShopState.Listings[0].BundlePrice = 100;
					++ShopRevision;
					PlayerShopWidget->RefreshFromPlayer();
					PlayerShopWidget->SelectOffer(ShopState.Listings[0].ListingId);
					if (Buy) Buy->OnClicked.Broadcast();
					Check(TEXT("shop unaffordable purchase reports failure without selling stock"), Buy && Buy->GetIsEnabled()
						&& CurrentGold == 0 && !ShopState.Listings[0].bSoldOut && Result && !Result->GetText().IsEmpty());
					ShopState.Listings[0].bSoldOut = true; ++ShopRevision;
					PlayerShopWidget->RefreshFromPlayer();
					PlayerShopWidget->SelectOffer(ShopState.Listings[0].ListingId);
					Check(TEXT("sold-out shop offer remains inspectable but cannot be purchased"), Buy && !Buy->GetIsEnabled());
				}
				if (!InventoryItems.IsEmpty())
				{
					InventoryItems[0].bLocked = true; ++EquipmentInventoryRevision;
					PlayerShopWidget->RefreshFromPlayer();
					PlayerShopWidget->SelectEquipmentForSale(InventoryItems[0].ItemId);
					PlayerShopWidget->RefreshFromPlayer();
					const UImage* Art = Cast<UImage>(Tree->FindWidget(TEXT("ShopSaleIcon")));
					Check(TEXT("locked shop equipment keeps its illustrated detail and disables both sale controls"), Art && Art->GetBrush().GetResourceObject()
						&& Sell && !Sell->GetIsEnabled() && SellAll && !SellAll->GetIsEnabled());
				}
				InventoryItems.Reset(); MaterialInventory.Reset();
				++EquipmentInventoryRevision; ++MaterialInventoryRevision;
				PlayerShopWidget->RefreshFromPlayer();
				const UImage* EmptyArt = Cast<UImage>(Tree->FindWidget(TEXT("ShopSaleIcon")));
				Check(TEXT("empty shop sale inventory hides stale art and disables sale"), EmptyArt && EmptyArt->GetVisibility() == ESlateVisibility::Hidden
					&& Sell && !Sell->GetIsEnabled() && SellAll && !SellAll->GetIsEnabled());
				ShopState = SavedShop; InventoryItems = SavedInventory; MaterialInventory = SavedMaterials; CurrentGold = SavedGold;
				++ShopRevision; ++EquipmentInventoryRevision; ++MaterialInventoryRevision;
				PlayerShopWidget->RefreshFromPlayer();
				UScrollBox* Catalog = Cast<UScrollBox>(Tree->FindWidget(TEXT("ShopOfferScroll")));
				UButton* Refresh = Cast<UButton>(Tree->FindWidget(TEXT("ShopRefreshButton")));
				if (Catalog) Catalog->SetScrollOffset(300);
				CurrentGold = 100000;
				if (Refresh) Refresh->OnClicked.Broadcast();
				Check(TEXT("manual shop refresh reveals the first new offer"), Catalog && Refresh
					&& Catalog->GetScrollOffset() == 0 && ShopState.RefreshSerial > SavedShop.RefreshSerial);
				ShopState = SavedShop; CurrentGold = SavedGold; ++ShopRevision;
				PlayerShopWidget->RefreshFromPlayer();
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
	At(12.6f, [this, Check, Shot] {
		Check(TEXT("ascension remains separate without pausing"), bAscensionOpen
			&& !bManagementInterfaceOpen && !UGameplayStatics::IsGamePaused(this));
		UWidgetTree* Tree = PlayerAscensionWidget->WidgetTree;
		const USizeBox* Root = Cast<USizeBox>(Tree->RootWidget);
		Check(TEXT("ascension uses the large 1600x600 canvas"), Root && Root->GetWidthOverride() == 1600 && Root->GetHeightOverride() == 600);
		bool bArt = true;
		for (const TCHAR* Name : {TEXT("AscensionBattleArt"),TEXT("AscensionEnlightenmentArt"),TEXT("AscensionFortuneArt"),
			TEXT("AscensionSealArt"),TEXT("AscensionAltarArt"),TEXT("AscensionCycleArt")})
		{
			const UImage* Art = Cast<UImage>(Tree->FindWidget(Name));
			bArt &= Art && Art->GetBrush().GetResourceObject() && FBox2f(Art->GetBrush().GetUVRegion()).bIsValid;
		}
		Check(TEXT("ascension displays all six original illustrations"), bArt);
		const UButton* Ascend = Cast<UButton>(Tree->FindWidget(TEXT("AscensionPerformButton")));
		const UButton* Cancel = Cast<UButton>(Tree->FindWidget(TEXT("AscensionCancelButton")));
		Check(TEXT("ineligible ascension is disabled without a pending confirmation"),
			Ascend && !Ascend->GetIsEnabled() && Cancel && Cancel->GetVisibility() == ESlateVisibility::Hidden);
		const auto SavedState = AscensionState;
		AscensionState.ImmortalSeals = 0;
		AscensionState.BattlePathRank = 0;
		AscensionState.EnlightenmentPathRank = 0;
		AscensionState.FortunePathRank = 0;
		PlayerAscensionWidget->RefreshFromPlayer();
		bool bZeroDisabled = true;
		for (const TCHAR* Name : {TEXT("AscensionBattlePathButton"),TEXT("AscensionEnlightenmentPathButton"),TEXT("AscensionFortunePathButton")})
		{
			const UButton* Button = Cast<UButton>(Tree->FindWidget(Name));
			bZeroDisabled &= Button && !Button->GetIsEnabled();
		}
		Check(TEXT("zero seals disable all three investment actions"), bZeroDisabled);
		AscensionState.ImmortalSeals = 100000;
		AscensionState.BattlePathRank = 50;
		AscensionState.EnlightenmentPathRank = 50;
		AscensionState.FortunePathRank = 50;
		PlayerAscensionWidget->RefreshFromPlayer();
		bool bMaxDisabled = true;
		for (const TCHAR* Name : {TEXT("AscensionBattlePathButton"),TEXT("AscensionEnlightenmentPathButton"),TEXT("AscensionFortunePathButton")})
		{
			const UButton* Button = Cast<UButton>(Tree->FindWidget(Name));
			bMaxDisabled &= Button && !Button->GetIsEnabled();
		}
		Check(TEXT("maxed ascension paths cannot consume further seals"), bMaxDisabled);
		AscensionState = SavedState;
		PlayerAscensionWidget->RefreshFromPlayer();
		const UImage* Sequence = Cast<UImage>(Tree->FindWidget(TEXT("AscensionSequenceImage")));
		const UCanvasPanelSlot* SequenceSlot = Sequence ? Cast<UCanvasPanelSlot>(Sequence->Slot) : nullptr;
		Check(TEXT("ascension animation preserves cropped aspect ratio"), SequenceSlot && SequenceSlot->GetSize() == FVector2D(128,430));
		Shot(TEXT("Ascension"));
	});
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
