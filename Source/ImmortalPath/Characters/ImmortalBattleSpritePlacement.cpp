#include "ImmortalBattleSpritePlacement.h"

#include "Components/CapsuleComponent.h"
#include "PaperCharacter.h"
#include "PaperFlipbookComponent.h"

float ImmortalBattleSpritePlacement::GetReferenceGroundOffset(const APaperCharacter& Reference)
{
	const UCapsuleComponent* Capsule = Reference.GetCapsuleComponent();
	const UPaperFlipbookComponent* Sprite = Reference.GetSprite();
	if (!Capsule || !Sprite)
	{
		return 0.0f;
	}
	return Sprite->GetComponentLocation().Z
		- (Capsule->GetComponentLocation().Z - Capsule->GetScaledCapsuleHalfHeight());
}

bool ImmortalBattleSpritePlacement::AlignToReferenceFootPlane(
	APaperCharacter& Actor, const APaperCharacter& Reference)
{
	const UCapsuleComponent* Capsule = Actor.GetCapsuleComponent();
	const UCapsuleComponent* ReferenceCapsule = Reference.GetCapsuleComponent();
	const UPaperFlipbookComponent* ReferenceSprite = Reference.GetSprite();
	UPaperFlipbookComponent* Sprite = Actor.GetSprite();
	if (!Capsule || !ReferenceCapsule || !ReferenceSprite || !Sprite)
	{
		return false;
	}
	FVector Location = Sprite->GetComponentLocation();
	Location.Y = ReferenceSprite->GetComponentLocation().Y;
	Location.Z = Capsule->GetComponentLocation().Z - Capsule->GetScaledCapsuleHalfHeight()
		+ GetReferenceGroundOffset(Reference);
	Sprite->SetWorldLocation(Location);
	return true;
}
