// Copyright RadioGuesser. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/EngineTypes.h"     // FTimerHandle
#include "RGHUDSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTimerTick, float, SecondsRemaining);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTimerExpired);

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
