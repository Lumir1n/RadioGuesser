// Copyright RadioGuesser. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RGHUDSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTimerTick,       float, SecondsRemaining);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTimerExpired);

/**
 * URGHUDSubsystem
 *
 * Manages the round countdown timer and HUD notification events.
 * Widgets bind to delegates here rather than polling every frame.
 */
UCLASS()
class RADIOGUESSER_API URGHUDSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ── Timer ─────────────────────────────────────────────────────────────────

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void StartRoundTimer(float DurationSeconds);

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void StopTimer();

    UFUNCTION(BlueprintPure, Category = "HUD")
    float GetTimeRemaining() const { return TimeRemaining; }

    UFUNCTION(BlueprintPure, Category = "HUD")
    bool IsTimerRunning() const { return bTimerRunning; }

    // ── Events ────────────────────────────────────────────────────────────────

    /** Fires every second with remaining time */
    UPROPERTY(BlueprintAssignable, Category = "HUD")
    FOnTimerTick OnTimerTick;

    /** Fires when timer reaches 0 */
    UPROPERTY(BlueprintAssignable, Category = "HUD")
    FOnTimerExpired OnTimerExpired;

private:
    void TickTimer();

    float TimeRemaining = 0.0f;
    bool  bTimerRunning = false;
    FTimerHandle TimerHandle;
};
