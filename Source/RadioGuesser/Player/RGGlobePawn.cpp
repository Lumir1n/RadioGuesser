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

    // Drag: Boolean fires Started on press, Completed on release — correct
    IA_Drag   = NewObject<UInputAction>(this, TEXT("IA_GlobeDrag"));
    IA_Drag->ValueType = EInputActionValueType::Boolean;

    // Zoom: 1D axis from mouse wheel
    IA_Zoom   = NewObject<UInputAction>(this, TEXT("IA_GlobeZoom"));
    IA_Zoom->ValueType = EInputActionValueType::Axis1D;

    // Mouse delta: 2D axis, fires every frame the mouse moves
    IA_MouseXY = NewObject<UInputAction>(this, TEXT("IA_GlobeMouseXY"));
    IA_MouseXY->ValueType = EInputActionValueType::Axis2D;

    // Left mouse button → drag toggle
    MappingContext->MapKey(IA_Drag,    EKeys::LeftMouseButton);
    // Mouse wheel → zoom
    MappingContext->MapKey(IA_Zoom,    EKeys::MouseWheelAxis);
    // Mouse movement → delta (always captured, we gate on bIsDragging in Tick)
    MappingContext->MapKey(IA_MouseXY, EKeys::Mouse2D);

    // Priority 0 — same as game IMC so globe controls always respond.
    // (In Enhanced Input, higher number = higher priority, so 0 is the base level.
    //  We use 0 here and let the game IMC also sit at 0; they don't conflict
    //  because they bind different actions.)
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (ULocalPlayer* LP = PC->GetLocalPlayer())
        {
            if (auto* Sys = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
            {
                Sys->AddMappingContext(MappingContext, 0);
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

    // Apply rotation while dragging — this pans the map by rotating the
    // spring arm pivot. From a top-down perspective this feels like dragging
    // the map underneath the camera (pan), not orbiting a globe.
    if (bIsDragging && !LastMouseDelta.IsNearlyZero())
    {
        const FRotator CurrentRot = SpringArm->GetRelativeRotation();

        // Invert X so dragging right moves the map right (natural pan feel)
        float NewYaw   = CurrentRot.Yaw   - LastMouseDelta.X * RotationSpeed;
        float NewPitch = FMath::Clamp(
            CurrentRot.Pitch + LastMouseDelta.Y * RotationSpeed,
            -89.0f, -15.0f);

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
    // Nothing needed here — drag state is tracked via bIsDragging in Tick
}

void ARGGlobePawn::OnDragStopped(const FInputActionValue& /*Value*/)
{
    // Just stop dragging — click detection is handled by IA_MapClick in
    // ARGPlayerController so we don't need to forward it here.
    // (Formerly there was a click-vs-drag check here that caused double-firing.)
    bIsDragging = false;
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
