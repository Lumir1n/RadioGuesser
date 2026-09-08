// Copyright RadioGuesser. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Map/RGMapSubsystem.h"        // needed for FRGGeoCoordinate in UFUNCTION params
#include "RGPlayerController.generated.h"

/**
 * ARGPlayerController
 *
 * Handles player input during a round:
 *  - map click → PlaceGuess
 *  - guess confirmation → SubmitGuess (sent to server via RPC)
 *  - radio controls delegated to URGRadioSubsystem
 */
UCLASS()
class RADIOGUESSER_API ARGPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ARGPlayerController();

    // ── Guess actions ─────────────────────────────────────────────────────────

    /**
     * Called by UI/map when the player clicks a location.
     * Places a tentative guess marker — does NOT submit it yet.
     */
    UFUNCTION(BlueprintCallable, Category = "Gameplay")
    void OnMapClicked(FRGGeoCoordinate Coordinate);

    /**
     * Called by UI "CONFIRM GUESS" button.
     * Sends the guess to the server via Server RPC.
     */
    UFUNCTION(BlueprintCallable, Category = "Gameplay")
    void ConfirmGuess();

    // ── Server RPC ────────────────────────────────────────────────────────────

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_SubmitGuess(FRGGeoCoordinate Coordinate, const FString& RoundToken);

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

private:
    /** True after ConfirmGuess() was called; prevents double submission */
    bool bGuessSubmitted = false;
};
