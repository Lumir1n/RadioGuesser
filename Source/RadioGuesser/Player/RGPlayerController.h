// Copyright RadioGuesser. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Map/RGMapSubsystem.h"
#include "RGPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

/**
 * ARGPlayerController
 *
 * Handles all player input during a round:
 *  - Left mouse click → raycast against Cesium globe → geographic coordinate
 *    → URGMapSubsystem::PlaceGuess (places tentative marker)
 *  - Confirm Guess action → URGMatchSubsystem::SubmitGuess (via HUD button or key)
 *  - Camera pan/zoom delegated to Blueprint (spring arm + mouse drag)
 *
 * Enhanced Input is used so key bindings are data-driven and remappable.
 */
UCLASS()
class RADIOGUESSER_API ARGPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ARGPlayerController();

    // ── Map interaction ───────────────────────────────────────────────────────

    /**
     * Called from Blueprint or directly — performs a line trace from the
     * mouse cursor against the Cesium globe and translates the hit point to
     * geographic coordinates, then calls URGMapSubsystem::PlaceGuess.
     */
    UFUNCTION(BlueprintCallable, Category = "Gameplay|Map")
    void TryPlaceGuessAtCursor();

    /**
     * Called by the HUD CONFIRM GUESS button or a keyboard binding.
     * Sends the pending guess to URGMatchSubsystem.
     */
    UFUNCTION(BlueprintCallable, Category = "Gameplay|Match")
    void ConfirmGuess();

    // ── Server RPC ────────────────────────────────────────────────────────────

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_SubmitGuess(FRGGeoCoordinate Coordinate, const FString& RoundToken);

    // ── Input setup ───────────────────────────────────────────────────────────

    /** Assign in BP_RadioGuesserPlayerController defaults — IMC_RadioGuesser */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    /** Left-click on globe — IA_MapClick */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> MapClickAction;

    /** Enter / Space to confirm guess — IA_ConfirmGuess */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> ConfirmGuessAction;

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

private:
    void OnMapClick(const FInputActionValue& Value);
    void OnConfirmGuess(const FInputActionValue& Value);

    bool bGuessSubmitted = false;
};
