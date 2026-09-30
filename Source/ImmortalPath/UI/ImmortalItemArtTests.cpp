#include "ImmortalInventorySlotWidget.h"
#include "ImmortalCraftingArt.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
namespace
{
	/** No project game instance, player, BeginPlay, ticking or save operations enter this context. */
	struct FItemArtTestContext
	{
		UWorld* World = nullptr;
		APlayerController* Controller = nullptr;
		TStrongObjectPtr<ULocalPlayer> LocalPlayer;

		FItemArtTestContext()
		{
			if (!GEngine) return;
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.InitializeScenes(false).AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false)
				.ShouldSimulatePhysics(false).SetTransactional(false).CreateFXSystem(false);
			World = UWorld::CreateWorld(EWorldType::Editor, false,
				MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("ItemArtTestWorld")),
				GetTransientPackage(), true, ERHIFeatureLevel::Num, &Values);
			if (!World) return;
			World->SetFlags(RF_Transient);
			GEngine->CreateNewWorldContext(EWorldType::Editor).SetCurrentWorld(World);
			FActorSpawnParameters Spawn;
			Spawn.ObjectFlags = RF_Transient;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Controller = World->SpawnActor<APlayerController>(APlayerController::StaticClass(), FTransform::Identity, Spawn);
			if (!Controller) return;
			LocalPlayer.Reset(NewObject<ULocalPlayer>(GEngine, NAME_None, RF_Transient));
			Controller->Player = LocalPlayer.Get();
			LocalPlayer->PlayerController = Controller;
			Controller->SetAsLocalPlayerController();
			World->AddController(Controller);
		}

		~FItemArtTestContext()
		{
			if (Controller)
			{
				Controller->Player = nullptr;
				if (LocalPlayer.IsValid()) LocalPlayer->PlayerController = nullptr;
				World->RemoveController(Controller);
				Controller->Destroy();
			}
			LocalPlayer.Reset();
			if (World)
			{
				World->DestroyWorld(false);
				GEngine->DestroyWorldContext(World);
			}
		}
	};

	bool IsVisible(const UWidget* Widget)
	{
		return Widget && Widget->GetVisibility() != ESlateVisibility::Collapsed && Widget->GetVisibility() != ESlateVisibility::Hidden;
	}

	/** Changes this test instance's existing configurable asset property, never a production test API or CDO. */
	bool RemoveAtlasFromTestInstance(UImmortalInventorySlotWidget* Widget, const FName PropertyName)
	{
		FSoftObjectProperty* Property = FindFProperty<FSoftObjectProperty>(Widget->GetClass(), PropertyName);
		if (!Property) return false;
		Property->SetPropertyValue_InContainer(Widget, FSoftObjectPtr());
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalInventoryItemArtTest, "ImmortalPath.UI.InventoryItemArt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalInventoryItemArtTest::RunTest(const FString& Parameters)
{
	FItemArtTestContext Context;
	if (!TestNotNull(TEXT("Transient UI test world exists"), Context.World)
		|| !TestNotNull(TEXT("Native local controller exists"), Context.Controller)) return false;
	TestNull(TEXT("Test context never initializes a project game instance"), Context.World->GetGameInstance());
	TestFalse(TEXT("Test context never starts gameplay"), Context.World->HasBegunPlay());
	TStrongObjectPtr<UImmortalInventorySlotWidget> Cell(CreateWidget<UImmortalInventorySlotWidget>(Context.Controller));
	if (!TestNotNull(TEXT("Real inventory cell is created"), Cell.Get())) return false;
	UImage* Art = Cast<UImage>(Cell->GetWidgetFromName(TEXT("InventoryItemIcon")));
	UImage* Frame = Cast<UImage>(Cell->GetWidgetFromName(TEXT("InventoryQualityFrame")));
	UTextBlock* Glyph = Cast<UTextBlock>(Cell->GetWidgetFromName(TEXT("InventoryMaterialGlyph")));
	UTextBlock* SlotLabel = Cast<UTextBlock>(Cell->GetWidgetFromName(TEXT("InventorySlotLabel")));
	UTextBlock* Level = Cast<UTextBlock>(Cell->GetWidgetFromName(TEXT("InventoryItemLevel")));
	UTextBlock* Lock = Cast<UTextBlock>(Cell->GetWidgetFromName(TEXT("InventoryLockGlyph")));
	UButton* Button = Cast<UButton>(Cell->GetWidgetFromName(TEXT("InventorySlotButton")));
	if (!TestNotNull(TEXT("Initialized tree has item art"), Art)
		|| !TestNotNull(TEXT("Initialized tree has a quality frame"), Frame)
		|| !TestNotNull(TEXT("Initialized tree has fallback glyph"), Glyph)
		|| !TestNotNull(TEXT("Initialized tree has slot badge"), SlotLabel)
		|| !TestNotNull(TEXT("Initialized tree has count/level"), Level)
		|| !TestNotNull(TEXT("Initialized tree has lock badge"), Lock)
		|| !TestNotNull(TEXT("Initialized tree has a real button"), Button)) return false;
	TestNotNull(TEXT("Fallback glyph remains attached to the widget tree"), Glyph->GetParent());
	TestNotNull(TEXT("Slot badge remains attached to the widget tree"), SlotLabel->GetParent());

	const auto CheckArtOnly = [this, Art, Glyph](const FString& Label, const FSlateBrush& Expected)
	{
		TestTrue(Label + TEXT(" art is visible"), IsVisible(Art));
		TestFalse(Label + TEXT(" fallback does not overlap art"), IsVisible(Glyph));
		TestNotNull(Label + TEXT(" art has an actual resource"), Art->GetBrush().GetResourceObject());
		TestTrue(Label + TEXT(" uses its shared atlas"), Art->GetBrush().GetResourceObject() == Expected.GetResourceObject());
		const FBox2f ActualUV = Art->GetBrush().GetUVRegion();
		const FBox2f ExpectedUV = Expected.GetUVRegion();
		TestTrue(Label + TEXT(" shows the intended cell"), ActualUV.bIsValid && ExpectedUV.bIsValid
			&& ActualUV.Min.Equals(ExpectedUV.Min, 0.00001f) && ActualUV.Max.Equals(ExpectedUV.Max, 0.00001f));
	};
	const auto CheckEmpty = [this, Art, Frame, Glyph, SlotLabel, Level, Lock, Button](const FString& Label)
	{
		TestFalse(Label + TEXT(" clears image"), IsVisible(Art));
		TestFalse(Label + TEXT(" clears fallback"), IsVisible(Glyph));
		TestFalse(Label + TEXT(" clears slot badge"), IsVisible(SlotLabel));
		TestFalse(Label + TEXT(" clears frame"), IsVisible(Frame));
		TestFalse(Label + TEXT(" clears count"), IsVisible(Level));
		TestTrue(Label + TEXT(" clears count text"), Level->GetText().IsEmpty());
		TestFalse(Label + TEXT(" clears lock"), IsVisible(Lock));
		TestFalse(Label + TEXT(" disables item action"), Button->GetIsEnabled());
		TestTrue(Label + TEXT(" clears tooltip"), Button->GetToolTipText().IsEmpty());
	};

	UTexture2D* EquipmentAtlas = LoadObject<UTexture2D>(nullptr,
		TEXT("/Game/GAME/Asset/DesktopPixelV2/UI/T_EquipmentAtlas.T_EquipmentAtlas"));
	UTexture2D* ForgeAtlas = LoadObject<UTexture2D>(nullptr,
		TEXT("/Game/GAME/Asset/DesktopPixelV2/UI/T_ForgeAtlas.T_ForgeAtlas"));
	UTexture2D* MaterialAtlas = LoadObject<UTexture2D>(nullptr,
		TEXT("/Game/GAME/Asset/DesktopPixelV2/UI/T_MaterialAtlas.T_MaterialAtlas"));
	UTexture2D* AlchemyAtlas = LoadObject<UTexture2D>(nullptr,
		TEXT("/Game/GAME/Asset/DesktopPixelV2/UI/T_AlchemyAtlas.T_AlchemyAtlas"));
	if (!TestNotNull(TEXT("Equipment atlas is available"), EquipmentAtlas)
		|| !TestNotNull(TEXT("Forge atlas is available"), ForgeAtlas)
		|| !TestNotNull(TEXT("Material atlas is available"), MaterialAtlas)
		|| !TestNotNull(TEXT("Alchemy atlas is available"), AlchemyAtlas)) return false;

	const EImmortalEquipmentSlot Slots[] = {EImmortalEquipmentSlot::Weapon, EImmortalEquipmentSlot::Head,
		EImmortalEquipmentSlot::Chest, EImmortalEquipmentSlot::Bracers, EImmortalEquipmentSlot::Belt,
		EImmortalEquipmentSlot::Boots, EImmortalEquipmentSlot::RingLeft, EImmortalEquipmentSlot::RingRight,
		EImmortalEquipmentSlot::Accessory};
	TSet<FIntPoint> ShownEquipmentCells;
	FImmortalEquipmentItem Equipment;
	Equipment.ItemId = FGuid::NewGuid();
	Equipment.DisplayName = TEXT("图标测试装备");
	Equipment.Quality = EImmortalEquipmentQuality::Legendary;
	Equipment.ItemLevel = 37;
	Equipment.bLocked = true;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Slots); ++Index)
	{
		Equipment.Slot = Slots[Index];
		Cell->InitializeSlot(nullptr, Equipment, true, true, true);
		const FString Label = UImmortalEquipmentLibrary::GetSlotText(Slots[Index]).ToString();
		CheckArtOnly(Label, ImmortalCraftingArt::EquipmentBrush(EquipmentAtlas, Slots[Index]));
		const FBox2f UV = Art->GetBrush().GetUVRegion();
		ShownEquipmentCells.Add(FIntPoint(FMath::RoundToInt(UV.Min.X * 3), FMath::RoundToInt(UV.Min.Y * 3)));
		TestTrue(Label + TEXT(" source column"), FMath::IsNearlyEqual(UV.Min.X, float(Index % 3) / 3, 0.00001f));
		TestTrue(Label + TEXT(" source row"), FMath::IsNearlyEqual(UV.Min.Y, float(Index / 3) / 3, 0.00001f));
		TestTrue(Label + TEXT(" quality outline visible"), IsVisible(Frame));
		TestTrue(Label + TEXT(" correct quality color"), Frame->GetBrush().OutlineSettings.Color.GetSpecifiedColor()
			.Equals(UImmortalEquipmentLibrary::GetQualityColor(Equipment.Quality)));
		TestTrue(Label + TEXT(" lock visible"), IsVisible(Lock));
		TestEqual(Label + TEXT(" item level"), Level->GetText().ToString(), FString(TEXT("37")));
		TestTrue(Label + TEXT(" selected style"), Button->GetStyle().Normal.TintColor.GetSpecifiedColor()
			.Equals(FLinearColor(0.25f, 0.22f, 0.13f)));
		TestTrue(Label + TEXT(" equipped tooltip"), Button->GetToolTipText().ToString().Contains(TEXT("已穿戴")));
		const TCHAR* Badge = Slots[Index] == EImmortalEquipmentSlot::Bracers ? TEXT("腕")
			: Slots[Index] == EImmortalEquipmentSlot::Belt ? TEXT("带")
			: Slots[Index] == EImmortalEquipmentSlot::RingLeft ? TEXT("戒1")
			: Slots[Index] == EImmortalEquipmentSlot::RingRight ? TEXT("戒2") : TEXT("");
		TestEqual(Label + TEXT(" slot badge visibility"), IsVisible(SlotLabel), *Badge != '\0');
		TestEqual(Label + TEXT(" slot badge text"), SlotLabel->GetText().ToString(), FString(Badge));
		TestTrue(Label + TEXT(" small badge remains readable"), SlotLabel->GetFont().Size >= 10 && SlotLabel->GetFont().Size <= 14);
	}
	TestEqual(TEXT("All nine equipment cells shown independently"), ShownEquipmentCells.Num(), 9);
	Cell->InitializeSlot(nullptr, FImmortalEquipmentItem(), false, false, false);
	CheckEmpty(TEXT("Empty equipment after a locked selected item"));
	Cell->InitializeSlot(nullptr, FImmortalEquipmentItem(), false, false, false, EImmortalEquipmentSlot::RingRight);
	TestTrue(TEXT("Empty paper-doll cell shows its faded placeholder"), IsVisible(Art)
		&& Art->GetBrush().TintColor.GetSpecifiedColor().A < 0.5f);
	TestEqual(TEXT("Empty paper-doll ring remains distinguishable"), SlotLabel->GetText().ToString(), FString(TEXT("戒2")));
	TestFalse(TEXT("Placeholder does not have an item quality frame"), IsVisible(Frame));
	TestFalse(TEXT("Placeholder is not an equipment action"), Button->GetIsEnabled());

	for (const FName Id : UImmortalMaterialLibrary::GetKnownMaterialIds())
	{
		FImmortalMaterialStack Stack;
		Stack.MaterialId = Id;
		Stack.Quantity = 23;
		Cell->InitializeMaterialSlot(nullptr, Stack, false);
		const FString Label = Id.ToString();
		CheckArtOnly(Label, ImmortalCraftingArt::MaterialBrush(ForgeAtlas, MaterialAtlas, Id));
		TestEqual(Label + TEXT(" material count"), Level->GetText().ToString(), FString(TEXT("×23")));
		TestFalse(Label + TEXT(" material is not locked equipment"), IsVisible(Lock));
		TestFalse(Label + TEXT(" material has no equipment quality"), IsVisible(Frame));
		TestFalse(Label + TEXT(" material clears equipment slot badge"), IsVisible(SlotLabel));
	}
	Cell->InitializeMaterialSlot(nullptr, FImmortalMaterialStack(), false);
	CheckEmpty(TEXT("Empty material after equipment and materials"));

	for (const FName Id : UImmortalAlchemyLibrary::GetKnownRecipeIds())
	{
		for (const EImmortalPillQuality Quality : {EImmortalPillQuality::Ordinary, EImmortalPillQuality::Exceptional})
		{
			FImmortalPillStack Stack;
			Stack.PillId = Id;
			Stack.Quality = Quality;
			Stack.Quantity = 12;
			Cell->InitializePillSlot(nullptr, Stack, false);
			const FString Label = Id.ToString() + UImmortalAlchemyLibrary::GetQualityText(Quality).ToString();
			CheckArtOnly(Label, ImmortalAlchemyArt::Brush(AlchemyAtlas, Id));
			TestTrue(Label + TEXT(" quality frame"), IsVisible(Frame)
				&& Frame->GetBrush().OutlineSettings.Color.GetSpecifiedColor().Equals(UImmortalAlchemyLibrary::GetQualityColor(Quality)));
			TestEqual(Label + TEXT(" quantity"), Level->GetText().ToString(), FString(TEXT("×12")));
			TestFalse(Label + TEXT(" clears equipment lock"), IsVisible(Lock));
		}
	}
	Cell->InitializePillSlot(nullptr, FImmortalPillStack(), false);
	CheckEmpty(TEXT("Empty pill after materials and pills"));

	for (const FName Id : UImmortalInventoryLibrary::GetKnownQuestItemIds())
	{
		FImmortalQuestItemDefinition Definition;
		if (!TestTrue(TEXT("Quest item definition exists"), UImmortalInventoryLibrary::GetQuestItemDefinition(Id, Definition))) continue;
		FImmortalQuestItemStack Stack;
		Stack.QuestItemId = Id;
		Stack.Quantity = 2;
		Cell->InitializeQuestItemSlot(nullptr, Stack, false);
		TestTrue(Id.ToString() + TEXT(" explicit quest glyph is on tree"), IsVisible(Glyph) && Glyph->GetParent());
		TestEqual(Id.ToString() + TEXT(" explicit quest glyph"), Glyph->GetText().ToString(), Definition.IconGlyph.ToString());
		TestFalse(Id.ToString() + TEXT(" quest glyph not overlapped by art"), IsVisible(Art));
		TestFalse(Id.ToString() + TEXT(" quest has no equipment frame"), IsVisible(Frame));
		TestTrue(Id.ToString() + TEXT(" quest glyph readable size"), Glyph->GetFont().Size >= 22);
		TestEqual(Id.ToString() + TEXT(" quest quantity"), Level->GetText().ToString(), FString(TEXT("×2")));
	}
	Cell->InitializeQuestItemSlot(nullptr, FImmortalQuestItemStack(), false);
	CheckEmpty(TEXT("Empty quest after pills and quests"));

	for (const FName Id : UImmortalArtifactLibrary::GetKnownArtifactIds())
	{
		FImmortalArtifactDefinition Definition;
		if (!TestTrue(TEXT("Artifact definition exists"), UImmortalArtifactLibrary::GetArtifactDefinition(Id, Definition))) continue;
		FImmortalArtifactItem Item = UImmortalArtifactLibrary::CreateArtifact(Id);
		Item.Level = 15;
		Item.Stars = 3;
		Item.bLocked = true;
		Cell->InitializeArtifactSlot(nullptr, Item, true, false);
		TestTrue(Id.ToString() + TEXT(" explicit artifact fallback is on tree"), IsVisible(Glyph) && Glyph->GetParent());
		TestEqual(Id.ToString() + TEXT(" artifact catalog glyph"), Glyph->GetText().ToString(), Definition.IconGlyph.ToString());
		TestFalse(Id.ToString() + TEXT(" artifact fallback has no stacked art"), IsVisible(Art));
		TestTrue(Id.ToString() + TEXT(" artifact quality frame"), IsVisible(Frame)
			&& Frame->GetBrush().OutlineSettings.Color.GetSpecifiedColor().Equals(UImmortalArtifactLibrary::GetQualityColor(Definition.Quality)));
		TestTrue(Id.ToString() + TEXT(" artifact lock visible"), IsVisible(Lock));
		TestEqual(Id.ToString() + TEXT(" artifact level and stars"), Level->GetText().ToString(), FString(TEXT("Lv15 ★3")));
	}
	Cell->InitializeArtifactSlot(nullptr, FImmortalArtifactItem(), false, false);
	TestTrue(TEXT("Empty artifact is an explicit category entry"), Button->GetIsEnabled() && IsVisible(Glyph)
		&& Glyph->GetText().ToString() == TEXT("法"));
	TestFalse(TEXT("Empty artifact clears quality"), IsVisible(Frame));
	TestFalse(TEXT("Empty artifact clears level"), IsVisible(Level));
	TestFalse(TEXT("Empty artifact clears lock"), IsVisible(Lock));
	Cell->InitializeSlot(nullptr, FImmortalEquipmentItem(), false, false, false);
	CheckEmpty(TEXT("Same cell clears every prior category"));

	// Missing textures must expose a useful attached glyph, rather than a blank icon or two overlapping marks.
	for (const FName PropertyName : {FName(TEXT("EquipmentAtlas")), FName(TEXT("ForgeAtlas")),
		FName(TEXT("MaterialAtlas")), FName(TEXT("AlchemyAtlas"))})
		TestTrue(TEXT("Test instance can simulate missing configurable atlas"), RemoveAtlasFromTestInstance(Cell.Get(), PropertyName));
	Equipment.Slot = EImmortalEquipmentSlot::Bracers;
	Cell->InitializeSlot(nullptr, Equipment, true, false, false);
	TestFalse(TEXT("Missing equipment atlas hides art"), IsVisible(Art));
	TestTrue(TEXT("Missing equipment atlas keeps fallback on the tree"), IsVisible(Glyph) && Glyph->GetParent());
	TestEqual(TEXT("Missing bracer art fallback"), Glyph->GetText().ToString(), FString(TEXT("腕")));
	FImmortalMaterialStack Material;
	Material.MaterialId = TEXT("SpiritGrass");
	Material.Quantity = 4;
	Cell->InitializeMaterialSlot(nullptr, Material, false);
	TestFalse(TEXT("Missing material atlas hides art"), IsVisible(Art));
	TestTrue(TEXT("Missing material atlas exposes a readable glyph"), IsVisible(Glyph) && !Glyph->GetText().IsEmpty());
	TestFalse(TEXT("Material fallback clears slot badge and lock"), IsVisible(SlotLabel) || IsVisible(Lock));
	FImmortalPillStack Pill;
	Pill.PillId = TEXT("HealingPill");
	Pill.Quantity = 5;
	Cell->InitializePillSlot(nullptr, Pill, false);
	TestFalse(TEXT("Missing pill atlas hides art"), IsVisible(Art));
	TestTrue(TEXT("Missing pill atlas exposes a readable glyph"), IsVisible(Glyph) && !Glyph->GetText().IsEmpty());
	TestTrue(TEXT("Missing pill atlas still shows quality"), IsVisible(Frame));
	Cell->InitializeSlot(nullptr, FImmortalEquipmentItem(), false, false, false);
	CheckEmpty(TEXT("Reused cell clears fallback state"));
	return true;
}
#endif
