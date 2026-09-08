// Copyright RadioGuesser. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RGRadioCatalogTypes.generated.h"

/**
 * FRGRadioStation
 *
 * Internal normalised station model.
 * NEVER contains live coordinates during active competitive gameplay
 * (those are kept server-side and only revealed after a round ends).
 */
USTRUCT(BlueprintType)
struct RADIOGUESSER_API FRGRadioStation
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString Id;
    UPROPERTY(BlueprintReadOnly) FString ExternalId;
    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) FString CountryCode;
    UPROPERTY(BlueprintReadOnly) FString Language;
    UPROPERTY(BlueprintReadOnly) TArray<FString> Tags;
    UPROPERTY(BlueprintReadOnly) FString Genre;
    UPROPERTY(BlueprintReadOnly) FString Homepage;
    UPROPERTY(BlueprintReadOnly) FString FaviconUrl;
    UPROPERTY(BlueprintReadOnly) FString StreamUrl;
    UPROPERTY(BlueprintReadOnly) int32   Bitrate = 0;
    UPROPERTY(BlueprintReadOnly) FString Codec;
    UPROPERTY(BlueprintReadOnly) bool    bIsActive = false;
};

UENUM(BlueprintType)
enum class ERGRadioHealthStatus : uint8
{
    Unknown                UMETA(DisplayName = "Unknown"),
    Active                 UMETA(DisplayName = "Active"),
    TemporarilyUnavailable UMETA(DisplayName = "Temporarily Unavailable"),
    Failed                 UMETA(DisplayName = "Failed"),
    Blocked                UMETA(DisplayName = "Blocked"),
};
