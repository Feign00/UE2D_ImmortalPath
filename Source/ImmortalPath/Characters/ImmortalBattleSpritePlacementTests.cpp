#include "ImmortalBattleSpritePlacement.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "PaperCharacter.h"
#include "PaperFlipbookComponent.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
namespace
{
	/** No game instance, project character, BeginPlay, ticking or save operation enters this world. */
	struct FBattleSpritePlacementTestWorld
	{
		UWorld* World = nullptr;

		FBattleSpritePlacementTestWorld()
		{
			if (!GEngine) return;
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.InitializeScenes(false).AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false)
				.ShouldSimulatePhysics(false).SetTransactional(false).CreateFXSystem(false);
			World = UWorld::CreateWorld(EWorldType::Editor, false,
				MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("BattleSpritePlacementTestWorld")),
				GetTransientPackage(), true, ERHIFeatureLevel::Num, &Values);
			if (!World) return;
			World->SetFlags(RF_Transient);
			GEngine->CreateNewWorldContext(EWorldType::Editor).SetCurrentWorld(World);
		}

		~FBattleSpritePlacementTestWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
				GEngine->DestroyWorldContext(World);
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalBattleSpritePlacementTest, "ImmortalPath.Art.BattleSpritePlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalBattleSpritePlacementTest::RunTest(const FString& Parameters)
{
	FBattleSpritePlacementTestWorld Context;
	if (!TestNotNull(TEXT("Transient editor world exists"), Context.World)) return false;
	TestNull(TEXT("Placement test never initializes a game instance"), Context.World->GetGameInstance());
	TestFalse(TEXT("Placement test never starts gameplay"), Context.World->HasBegunPlay());

	FActorSpawnParameters Spawn;
	Spawn.ObjectFlags = RF_Transient;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APaperCharacter* Actor = Context.World->SpawnActor<APaperCharacter>(APaperCharacter::StaticClass(),
		FTransform(FRotator::ZeroRotator, FVector(120.0, -50.0, 600.0), FVector(1.5, 1.25, 2.0)), Spawn);
	APaperCharacter* Reference = Context.World->SpawnActor<APaperCharacter>(APaperCharacter::StaticClass(),
		FTransform(FRotator::ZeroRotator, FVector(-200.0, 80.0, 450.0), FVector(2.0, 0.8, 1.5)), Spawn);
	if (!TestNotNull(TEXT("Plain PaperCharacter actor exists"), Actor)
		|| !TestNotNull(TEXT("Plain PaperCharacter reference exists"), Reference)) return false;
	UCapsuleComponent* Capsule = Actor->GetCapsuleComponent();
	UCapsuleComponent* ReferenceCapsule = Reference->GetCapsuleComponent();
	UPaperFlipbookComponent* Sprite = Actor->GetSprite();
	UPaperFlipbookComponent* ReferenceSprite = Reference->GetSprite();
	if (!TestNotNull(TEXT("Actor capsule exists"), Capsule)
		|| !TestNotNull(TEXT("Reference capsule exists"), ReferenceCapsule)
		|| !TestNotNull(TEXT("Actor sprite exists"), Sprite)
		|| !TestNotNull(TEXT("Reference sprite exists"), ReferenceSprite)) return false;

	Sprite->SetRelativeScale3D(FVector(0.5, 0.75, 1.25));
	Sprite->SetRelativeRotation(FRotator(0.0, 0.0, 12.0));
	Sprite->SetWorldLocation(FVector(333.0, 999.0, 5.0));
	const auto CheckPlacement = [this, Actor, Reference, Capsule, ReferenceCapsule, Sprite, ReferenceSprite]
		(const FString& Label, const double GroundOffset, const double ReferenceDepth)
	{
		const double ReferenceBottom = ReferenceCapsule->GetComponentLocation().Z
			- ReferenceCapsule->GetScaledCapsuleHalfHeight();
		ReferenceSprite->SetWorldLocation(FVector(-100.0, ReferenceDepth, ReferenceBottom + GroundOffset));
		const FTransform ActorBefore = Actor->GetActorTransform();
		const FTransform CapsuleBefore = Capsule->GetComponentTransform();
		const FTransform ReferenceBefore = Reference->GetActorTransform();
		const FTransform ReferenceSpriteBefore = ReferenceSprite->GetComponentTransform();
		const FVector SpriteScaleBefore = Sprite->GetRelativeScale3D();
		const FRotator SpriteRotationBefore = Sprite->GetRelativeRotation();
		const FVector SpriteBefore = Sprite->GetComponentLocation();
		const float RadiusBefore = Capsule->GetUnscaledCapsuleRadius();
		const float HalfHeightBefore = Capsule->GetUnscaledCapsuleHalfHeight();
		const ECollisionEnabled::Type CollisionBefore = Capsule->GetCollisionEnabled();
		const ECollisionChannel ObjectTypeBefore = Capsule->GetCollisionObjectType();
		const double ExpectedZ = CapsuleBefore.GetLocation().Z
			- Capsule->GetScaledCapsuleHalfHeight() + GroundOffset;

		TestTrue(Label + TEXT(" measures the actual visual ground offset"), FMath::IsNearlyEqual(
			double(ImmortalBattleSpritePlacement::GetReferenceGroundOffset(*Reference)), GroundOffset, 0.001));
		TestTrue(Label + TEXT(" alignment succeeds"), ImmortalBattleSpritePlacement::AlignToReferenceFootPlane(*Actor, *Reference));
		const FVector Aligned = Sprite->GetComponentLocation();
		TestTrue(Label + TEXT(" preserves world X"), FMath::IsNearlyEqual(Aligned.X, SpriteBefore.X, 0.001));
		TestTrue(Label + TEXT(" matches reference sprite world Y"), FMath::IsNearlyEqual(Aligned.Y, ReferenceDepth, 0.001));
		TestTrue(Label + TEXT(" places the foot at scaled capsule bottom plus visual offset"),
			FMath::IsNearlyEqual(Aligned.Z, ExpectedZ, 0.001));
		TestTrue(Label + TEXT(" actor transform remains unchanged"), Actor->GetActorTransform().Equals(ActorBefore, 0.001));
		TestTrue(Label + TEXT(" collision transform remains unchanged"), Capsule->GetComponentTransform().Equals(CapsuleBefore, 0.001));
		TestEqual(Label + TEXT(" collision radius remains unchanged"), Capsule->GetUnscaledCapsuleRadius(), RadiusBefore);
		TestEqual(Label + TEXT(" collision half height remains unchanged"), Capsule->GetUnscaledCapsuleHalfHeight(), HalfHeightBefore);
		TestTrue(Label + TEXT(" collision policy remains unchanged"), Capsule->GetCollisionEnabled() == CollisionBefore
			&& Capsule->GetCollisionObjectType() == ObjectTypeBefore);
		TestTrue(Label + TEXT(" sprite scale remains unchanged"), Sprite->GetRelativeScale3D().Equals(SpriteScaleBefore, 0.001));
		TestTrue(Label + TEXT(" sprite rotation remains unchanged"), Sprite->GetRelativeRotation().Equals(SpriteRotationBefore, 0.001));
		TestTrue(Label + TEXT(" reference actor remains unchanged"), Reference->GetActorTransform().Equals(ReferenceBefore, 0.001));
		TestTrue(Label + TEXT(" reference sprite remains unchanged"), ReferenceSprite->GetComponentTransform().Equals(ReferenceSpriteBefore, 0.001));
		TestTrue(Label + TEXT(" repeated alignment succeeds"), ImmortalBattleSpritePlacement::AlignToReferenceFootPlane(*Actor, *Reference));
		TestTrue(Label + TEXT(" repeated alignment never accumulates offset"), Sprite->GetComponentLocation().Equals(Aligned, 0.001));
	};

	CheckPlacement(TEXT("Non-unit actor/reference scales with lowered foot plane"), -115.0, 35.0);
	Actor->SetActorScale3D(FVector(0.65, 1.4, 1.75));
	Reference->SetActorScale3D(FVector(1.25, 1.8, 0.7));
	CheckPlacement(TEXT("Changed non-unit scales with raised foot plane"), 31.0, -145.0);
	TestNull(TEXT("Placement does not create a game instance"), Context.World->GetGameInstance());
	TestFalse(TEXT("Placement does not enter BeginPlay"), Context.World->HasBegunPlay());
	return true;
}
#endif
