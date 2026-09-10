// Copyright RadioGuesser. All Rights Reserved.

#include "Player/RGGlobePawn.h"
#include "RadioGuesser.h"
#include "Map/RGCesiumMapManager.h"
#include "Player/RGPlayerController.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"

ARGGlobePawn::ARGGlobePawn()
{
    PrimaryActorTick.bCanEverTick = true;

    GlobeRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GlobeRoot"));
    SetRootComponent(GlobeRoot);

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(GlobeRoot);
    SpringArm->TargetArmLength            = CurrentArmLength;
    SpringArm->bDoCollisionTest           = false;
    SpringArm->bEnableCameraLag           = true;
    SpringArm->CameraLagSpeed             = 8.0f;
    SpringArm->bInheritPitch              = true;
    SpringArm->bInheritYaw                = true;
    SpringArm->bInheritRoll               = false;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    Camera->bUsePawnControlRotation = false;

    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw   = false;
    bUseControllerRotationRoll  = false;
}

void ARGGlobePawn::BeginPlay()
{
    Super::BeginPlay();

    // Position pawn at globe center (origin)
    SetActorLocation(FVector::ZeroVector);

    // Set initial arm length
    SpringArm->TargetArmLength = CurrentArmLength;

    // Set initial camera angle — top-down view like GeoGuessr globe:
    // Pitch -70 = mostly top-down with slight angle to show globe curvature
    SpringArm->SetRelativeRotation(FRotator(-70.0f, 0.0f, 0.0f));

    // Auto-build a simple Input Mapping Context for globe camera controls
    // We create standalone actions so we don't conflict with the game IMC
    MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Globe"));

    IA_Drag   = NewObject<UInputAction>(this, TEXT("IA_GlobeDrag"));
    IA_Drag->ValueType = EInputActionValueType::Boolean;

    IA_Zoom   = NewObject<UInputAction>(this, TEXT("IA_GlobeZoom"));
    IA_Zoom->ValueType = EInputActionValueType::Axis1D;

    IA_MouseXY = NewObject<UInputAction>(this, TEXT("IA_GlobeMouseXY"));
    IA_MouseXY->ValueType = EInputActionValueType::Axis2D;

    // Map Left Mouse Button to drag
    FEnhancedActionKeyMapping DragMapping;
    DragMapping.Action = IA_Drag;
    DragMapping.Key    = EKeys::LeftMouseButton;
    MappingContext->MapKey(IA_Drag,    EKeys::LeftMouseButton);

    // Map mouse wheel to zoom
    MappingContext->MapKey(IA_Zoom,    EKeys::MouseWheelAxis);

    // Map mouse XY to delta
    MappingContext->MapKey(IA_MouseXY, EKeys::Mouse2D);

    // Register the context with lower priority than game IMC (priority 1)
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (ULocalPlayer* LP = PC->GetLocalPlayer())
        {
            if (auto* Sys = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
            {
                Sys->AddMappingContext(MappingContext, 1);
            }
        }
    }
}

void ARGGlobePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        EIC->BindAction(IA_Drag,    ETriggerEvent::Started,    this, &ARGGlobePawn::OnDragStarted);
        EIC->BindAction(IA_Drag,    ETriggerEvent::Ongoing,    this, &ARGGlobePawn::OnDragOngoing);
        EIC->BindAction(IA_Drag,    ETriggerEvent::Completed,  this, &ARGGlobePawn::OnDragStopped);
        EIC->BindAction(IA_Drag,    ETriggerEvent::Canceled,   this, &ARGGlobePawn::OnDragStopped);
        EIC->BindAction(IA_Zoom,    ETriggerEvent::Triggered,  this, &ARGGlobePawn::OnZoom);
        EIC->BindAction(IA_MouseXY, ETriggerEvent::Triggered,  this, &ARGGlobePawn::OnMouseXY);
    }
}

void ARGGlobePawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Smooth zoom
    if (SpringArm)
    {
        SpringArm->TargetArmLength = FMath::FInterpTo(
            SpringArm->TargetArmLength, CurrentArmLength, DeltaTime, 8.0f);
    }

    // Apply rotation while dragging
    if (bIsDragging && !LastMouseDelta.IsNearlyZero())
    {
        const FRotator CurrentRot = SpringArm->GetRelativeRotation();

        float NewPitch = FMath::Clamp(
            CurrentRot.Pitch - LastMouseDelta.Y * RotationSpeed,
            -89.0f, -15.0f);
        float NewYaw   = CurrentRot.Yaw + LastMouseDelta.X * RotationSpeed;

        SpringArm->SetRelativeRotation(FRotator(NewPitch, NewYaw, 0.0f));
        LastMouseDelta = FVector2D::ZeroVector;
    }
}

// ─── Input handlers ───────────────────────────────────────────────────────────

void ARGGlobePawn::OnDragStarted(const FInputActionValue& /*Value*/)
{
    bIsDragging     = true;
    TotalDragPixels = 0.0f;
    LastMouseDelta  = FVector2D::ZeroVector;
}

void ARGGlobePawn::OnDragOngoing(const FInputActionValue& /*Value*/)
{
    TotalDragPixels += LastMouseDelta.Size();
}

void ARGGlobePawn::OnDragStopped(const FInputActionValue& /*Value*/)
{
    const bool bWasClick = TotalDragPixels < ClickDragThreshold;
    bIsDragging = false;

    if (bWasClick)
    {
        // It was a click not a drag — forward to PlayerController for guess placement
        if (ARGPlayerController* PC = Cast<ARGPlayerController>(GetController()))
        {
            PC->TryPlaceGuessAtCursor();
        }
    }

    LastMouseDelta = FVector2D::ZeroVector;
}

void ARGGlobePawn::OnZoom(const FInputActionValue& Value)
{
    const float Axis = Value.Get<float>();
    CurrentArmLength = FMath::Clamp(
        CurrentArmLength - Axis * ZoomSpeed,
        MinArmLength,
        MaxArmLength);
}

void ARGGlobePawn::OnMouseXY(const FInputActionValue& Value)
{
    LastMouseDelta = Value.Get<FVector2D>();
}
