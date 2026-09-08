// Copyright RadioGuesser. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "RGGameInstance.generated.h"

/**
 * URGGameInstance
 *
 * Central GameInstance for RadioGuesser.
 * Subsystems (Radio, Map, Backend, Auth, etc.) are registered as
 * UGameInstanceSubsystem — this class acts only as a lightweight hub.
 */
UCLASS()
class RADIOGUESSER_API URGGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void Init() override;
    virtual void Shutdown() override;
};
