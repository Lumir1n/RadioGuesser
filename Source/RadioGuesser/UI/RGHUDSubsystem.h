// Copyright RadioGuesser. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RGHUDSubsystem.generated.h"

// FTimerHandle is in Engine module — available via CoreMinimal through Engine.h
// but we include the explicit header to be safe
#include "Engine/TimerHandle.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTimerTick,  float, SecondsRemaining);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTimerExpired);

/**
 * URGHUDSubsystem
 *
 * Manages the round countdown timer. Fires OnTimerTick every second
 * and OnTimerExpired when time runs out. Widgets bind to these delegates.
 */
UCLASS()
class RADIOGUESSER_API URGHUDSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void StartRoundTimer(float DurationSeconds);

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void StopTimer();

    UFUNCTION(BlueprintPure, Category = "HUD")
    float GetTimeRemaining() const { return TimeRemaining; }

    UFUNCTION(BlueprintPure, Category = "HUD")
    bool IsTimerRunning() const { return bTimerRunning; }

    UPROPERTY(BlueprintAssignable, Category = "HUD")
    FOnTimerTick OnTimerTick;

    UPROPERTY(BlueprintAssignable, Category = "HUD")
    FOnTimerExpired OnTimerExpired;

private:
    void TickTimer();

    float        TimeRemaining = 0.0f;
    bool         bTimerRunning = false;
    FTimerHandle TimerHandle;
};
