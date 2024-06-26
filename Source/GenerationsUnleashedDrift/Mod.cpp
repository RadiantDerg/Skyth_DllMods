// Mod.cpp

enum
{
    DRIFT_L,
    DRIFT_START_L,
    DRIFT_END_L,
    DRIFT_R,
    DRIFT_START_R,
    DRIFT_END_R,
    DRIFT_ANIM_COUNT
};

constexpr const char* SWA_ANIM_NAMES[DRIFT_ANIM_COUNT] =
{
    "DriftL_SWA",
    "DriftStartL_SWA",
    "DriftEndL_SWA",
    "DriftR_SWA",
    "DriftStartR_SWA",
    "DriftEndR_SWA",
};

constexpr const char* BB_ANIM_NAMES[DRIFT_ANIM_COUNT] =
{
    "DriftL_BlueBlur",
    "DriftStartL_BlueBlur",
    "DriftEndL_BlueBlur",
    "DriftR_BlueBlur",
    "DriftStartR_BlueBlur",
    "DriftEndR_BlueBlur",
};

constexpr const char* SWA_ANIM_FILE_NAMES[DRIFT_ANIM_COUNT] =
{
    "sn_drift_swa_l_loop",
    "sn_drift_swa_l_s",
    "sn_drift_swa_l_e",
    "sn_drift_swa_r_loop",
    "sn_drift_swa_r_s",
    "sn_drift_swa_r_e"
};

constexpr const char* BB_ANIM_FILE_NAMES[DRIFT_ANIM_COUNT] =
{
    "sn_drift_bb_l_loop",
    "sn_drift_bb_l_s",
    "sn_drift_bb_l_e",
    "sn_drift_bb_r_loop",
    "sn_drift_bb_r_s",
    "sn_drift_bb_r_e"
};

constexpr float BB_DRIFT_START_FINAL_FRAME = 16.0f;
constexpr float SWA_DRIFT_START_FINAL_FRAME = 20.0f;

// Generations: 26
// Unleashed: 26
constexpr float DRIFT_END_FINAL_FRAME = 26.0f;

const char* animNames[DRIFT_ANIM_COUNT];

// Generations: 16
// Unleashed: 20
float driftStartFinalFrame;

//~~~~~~~~~~~~~~~~~~~~~~~~~//
// Insert drift animations // 
//~~~~~~~~~~~~~~~~~~~~~~~~~//

HOOK(void*, __cdecl, InitializeSonicAnimationList, 0x1272490)
{
    void* result = originalInitializeSonicAnimationList();
    {
        CAnimationStateSet* set = (CAnimationStateSet*)0x15E8D40;
        CAnimationStateInfo* entries = new CAnimationStateInfo[set->count + 2 * DRIFT_ANIM_COUNT];

        std::copy(set->entries, set->entries + set->count, entries);

        for (size_t i = 0; i < 2 * DRIFT_ANIM_COUNT; i++)
        {
            const size_t animIndex = i % DRIFT_ANIM_COUNT;

            CAnimationStateInfo& entry = entries[set->count + i];

            entry.name = i >= DRIFT_ANIM_COUNT ? BB_ANIM_NAMES[animIndex] : SWA_ANIM_NAMES[animIndex];
            entry.fileName = i >= DRIFT_ANIM_COUNT ? BB_ANIM_FILE_NAMES[animIndex] : SWA_ANIM_FILE_NAMES[animIndex];
            entry.speed = 1.0f;
            entry.playbackType = animIndex == DRIFT_L || animIndex == DRIFT_R ? 0 : 1;
            entry.field10 = 0;
            entry.field14 = -1.0f;
            entry.field18 = -1.0f;
            entry.field1C = 0;
            entry.field20 = -1;
            entry.field24 = -1;
            entry.field28 = -1;
            entry.field2C = -1;
        }

        WRITE_MEMORY(&set->entries, void*, entries);
        WRITE_MEMORY(&set->count, size_t, set->count + 2 * DRIFT_ANIM_COUNT);
    }

    return result;
}

HOOK(void, __fastcall, CSonicCreateAnimationStates, 0xE1B6C0, void* This, void* Edx, void* A2, void* A3)
{
    originalCSonicCreateAnimationStates(This, Edx, A2, A3);

    FUNCTION_PTR(void*, __stdcall, createAnimationState, 0xCDFA20,
        void* This, boost::shared_ptr<void>& spAnimationState, const hh::base::CSharedString& name, const hh::base::CSharedString& alsoName);

    for (size_t i = 0; i < 2 * DRIFT_ANIM_COUNT; i++)
    {
        const size_t animIndex = i % DRIFT_ANIM_COUNT;
        const hh::base::CSharedString animName = i >= DRIFT_ANIM_COUNT ? BB_ANIM_NAMES[animIndex] : SWA_ANIM_NAMES[animIndex];

        boost::shared_ptr<void> animationState;
        createAnimationState(A2, animationState, animName, animName);
    }
}

HOOK(bool, __stdcall, ParseArchiveTree, 0xD4C8E0, void* A1, char* data, const size_t size, void* database)
{
    constexpr std::string_view str =
        "  <DefAppend>"
        "    <Name>Sonic</Name>"
        "    <Archive>SonicDrift</Archive>"
        "  </DefAppend>";

    const size_t newSize = size + str.size();
    const std::unique_ptr<char[]> buffer = std::make_unique<char[]>(newSize);
    memcpy(buffer.get(), data, size);

    char* insertionPos = strstr(buffer.get(), "<Include>");

    memmove(insertionPos + str.size(), insertionPos, size - (size_t)(insertionPos - buffer.get()));
    memcpy(insertionPos, str.data(), str.size());

    bool result;
    {
        result = originalParseArchiveTree(A1, buffer.get(), newSize, database);
    }

    return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// Use correct drift sound depending on what surface Sonic is on // 
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

uint32_t currentDriftCueId;

uint32_t __cdecl getDriftCueId()
{
    return Sonic::Player::CPlayerSpeedContext::GetInstance()->StateFlag(eStateFlag_OnWater) ? 2002059 : 2002039;
}

uint32_t CSonicStateDriftDriftingSoundMidAsmHookReturnAddress = 0xDF36A3;

void __declspec(naked) CSonicStateDriftDriftingSoundMidAsmHook()
{
    __asm
    {
        push ebx
        push ecx
        push edx

        call getDriftCueId
        mov currentDriftCueId, eax

        pop edx
        pop ecx
        pop ebx

        push eax

        jmp[CSonicStateDriftDriftingSoundMidAsmHookReturnAddress]
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// Implement Intro -> Loop -> Outro animation sequence //
// Restore footsteps when drifting                     // 
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

//
// Choose left/right intro animation appropriately
//

bool __fastcall ShouldDriftStartChangeAnimation()
{
    const auto& animName = Sonic::Player::CPlayerSpeedContext::GetInstance()->GetCurrentAnimationName();

    // Return false if current animation is already drift, but true if it's ending
    return (strstr(animName.c_str(), "Drift") == nullptr || strstr(animName.c_str(), "DriftEnd") != nullptr);
}

uint32_t CSonicStateDriftDriftingLoopAnimationMidAsmHookReturnAddress = 0xDF35F3;

void __declspec(naked) CSonicStateDriftDriftingLoopAnimationMidAsmHook()
{
    static size_t skipAnimDriftR = 0xDF3611;
    static size_t skipAnimDriftL = 0xDF3644;
    __asm
    {
        // check if change anim
        push ebx
        call [ShouldDriftStartChangeAnimation]
        pop ebx

        test al,al
        jnz changeAnim

        fldz
        fxch st(1)
        lea ecx, [esp + 0x40 - 0x2C]
        fcomip st, st(1)
        fstp st
        jbe skipDriftL
        jmp [skipAnimDriftR]
    skipDriftL:
        jmp [skipAnimDriftL]

    changeAnim:
        fldz
        fxch st(1)
        lea ecx, [esp + 0x40 - 0x2C]
        fcomip st, st(1)
        fstp st
        jbe pushDriftL

        push animNames[DRIFT_START_R * 4]
        jmp return

    pushDriftL:
        push animNames[DRIFT_START_L * 4]

    return:
        jmp[CSonicStateDriftDriftingLoopAnimationMidAsmHookReturnAddress]
    }
}

//
// Handle Loop -> Outro transition
// Implement footstep sounds
// Handle Water -> Ground drift sound transition
//

bool wasDriftRight = false;
float driftDirectionTime = 0.0f;

HOOK(void, __fastcall, CSonicStateDriftDriftingUpdate, 0xDF32D0, Hedgehog::Universe::TStateMachine<Sonic::Player::CPlayerSpeedContext::CStateSpeedBase>::TState* This)
{
    const auto playerContext = static_cast<Sonic::Player::CPlayerSpeedContext*>(This->GetContext()->GetContext());

    const uint32_t cueId = getDriftCueId();

    if (currentDriftCueId != cueId)
    {
        *(boost::shared_ptr<Hedgehog::Sound::CSoundHandle>*)((char*)This + 96) = playerContext->PlaySound(cueId, 1);
        currentDriftCueId = cueId;
    }

    originalCSonicStateDriftDriftingUpdate(This);

    const auto spAnimInfo = boost::make_shared<Sonic::Message::MsgGetAnimationInfo>();
    playerContext->m_pPlayer->SendMessageImm(playerContext->m_pPlayer->m_ActorID, spAnimInfo);

    if (strstr(spAnimInfo->m_Name.c_str(), "DriftStartL") != nullptr && spAnimInfo->m_Frame >= driftStartFinalFrame)
        playerContext->ChangeAnimation(animNames[DRIFT_L]);

    else if (strstr(spAnimInfo->m_Name.c_str(), "DriftStartR") != nullptr && spAnimInfo->m_Frame >= driftStartFinalFrame)
        playerContext->ChangeAnimation(animNames[DRIFT_R]);

    bool driftRight = playerContext->StateFlag(eStateFlag_DriftRight);

    if (wasDriftRight != driftRight)
    {
        // Reset timer if switching direction
        driftDirectionTime = 0.0f;
        wasDriftRight = driftRight;
    }
    else
    {
        driftDirectionTime += This->m_pStateMachine->m_UpdateInfo.DeltaTime;

        if (driftDirectionTime > 0.4f)
        {
            if (strstr(spAnimInfo->m_Name.c_str(), "DriftL") != nullptr && driftRight)
            {
                driftDirectionTime = 0.0f;
                playerContext->ChangeAnimation(animNames[DRIFT_START_R]);
            }
            else if (strstr(spAnimInfo->m_Name.c_str(), "DriftR") != nullptr && !driftRight)
            {
                driftDirectionTime = 0.0f;
                playerContext->ChangeAnimation(animNames[DRIFT_START_L]);
            }
        }
    }

    static float intervalTime = 0.0f;

    if (intervalTime > 0.07f)
    {
        FUNCTION_PTR(uint32_t, __thiscall, getSurfaceId, 0xDFD420, void* This, uint32_t type);
        playerContext->PlaySound(getSurfaceId(playerContext, 1), 1);

        intervalTime = 0.0f;
    }
    else
    {
        intervalTime += This->m_pStateMachine->m_UpdateInfo.DeltaTime;
    }
}

//
// Swap to outro animation when finishing drift
//

HOOK(void, __fastcall, CSonicStateDriftOnLeave, 0xDF2D20, Sonic::Player::CPlayerSpeedContext::CStateSpeedBase* This)
{
    driftDirectionTime = 0.0f;

    const auto& animName = This->GetContext()->GetCurrentAnimationName();

    if (animName == animNames[DRIFT_L] || animName == animNames[DRIFT_R])
    {
        This->GetContext()->ChangeAnimation(
            static_cast<Sonic::Player::CPlayerSpeedContext*>(This->GetContext())->StateFlag(eStateFlag_DriftRight)
                ? animNames[DRIFT_END_R]
                : animNames[DRIFT_END_L]);
    }

    originalCSonicStateDriftOnLeave(This);
}

//
// Make outro animation persist till it's done playing
//

HOOK(void, __stdcall, ChangeAnimation, 0xCDFC80, void* A1, boost::shared_ptr<void>& A2, const hh::base::CSharedString& name)
{
    if (name == "Walk")
    {
        const auto pPlayer = Sonic::Player::CPlayerSpeedContext::GetInstance()->m_pPlayer;

        const auto spAnimInfo = boost::make_shared<Sonic::Message::MsgGetAnimationInfo>();
        pPlayer->SendMessageImm(pPlayer->m_ActorID, spAnimInfo);

        if (strstr(spAnimInfo->m_Name.c_str(), "DriftEnd") != nullptr && spAnimInfo->m_Frame < DRIFT_END_FINAL_FRAME)
        {
            memset(&A2, 0, sizeof(A2));
            return;
        }
    }

    originalChangeAnimation(A1, A2, name);
}

//
// Swap to walk animation when outro is done
//

// TODO: Does not work properly with brake. Sonic gets stuck in walk animation when idling.

HOOK(void, __fastcall, CPlayerSpeedUpdate, 0xE6BF20, Sonic::Player::CPlayerSpeed* This, void* Edx, void* A2)
{
    if (!Sonic::Player::CSonicContext::GetInstance())
        return originalCPlayerSpeedUpdate(This, Edx, A2);

    const bool isSwa = This->m_spCharacterModel->GetNode("SonicRoot") != nullptr;
    memcpy(animNames, isSwa ? SWA_ANIM_NAMES : BB_ANIM_NAMES, sizeof(animNames));
    driftStartFinalFrame = isSwa ? SWA_DRIFT_START_FINAL_FRAME : BB_DRIFT_START_FINAL_FRAME;

    const auto spAnimInfo = boost::make_shared<Sonic::Message::MsgGetAnimationInfo>();
    This->SendMessageImm(This->m_ActorID, spAnimInfo);

    if (strstr(spAnimInfo->m_Name.c_str(), "DriftEnd") != nullptr && spAnimInfo->m_Frame >= DRIFT_END_FINAL_FRAME)
        This->GetContext()->ChangeAnimation("Walk");

    originalCPlayerSpeedUpdate(This, Edx, A2);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// Restore sea-spray sound when boosting on water // 
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
static boost::shared_ptr<Hedgehog::Sound::CSoundHandle> seaSpraySoundHandle;

HOOK(void, __fastcall, CSonicStatePluginOnWaterUpdate, 0x119BED0, Hedgehog::Universe::TStateMachine<Sonic::Player::CPlayerSpeedContext>::TState* This)
{
    if (This->GetContext()->StateFlag(eStateFlag_OnWater) && This->GetContext()->StateFlag(eStateFlag_Boost))
	{
		if (seaSpraySoundHandle == nullptr)
            seaSpraySoundHandle = This->GetContext()->PlaySound(2002059, 1);
	}
	else
	{
		seaSpraySoundHandle.reset();
	}

	originalCSonicStatePluginOnWaterUpdate(This);
}

// Fix dying while boosting on water doesn't destroy the sfx
HOOK(int, __fastcall, MsgRestartStage, 0xE76810, uint32_t* This, void* Edx, void* message)
{
    seaSpraySoundHandle.reset();
    return originalMsgRestartStage(This, Edx, message);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// Don't abruptly stop player when drift threshold isn't met //
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
uint32_t CSonicStateDriftStartUpdateMidAsmHookReturnAddress = 0xDF2D18;

void __declspec(naked) CSonicStateDriftStartUpdateMidAsmHook()
{
    __asm
    {
        mov edi, dword ptr[edi + 8]
        lea edi, [edi + 107]
        mov byte ptr[edi], 1
        jmp[CSonicStateDriftStartUpdateMidAsmHookReturnAddress]
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// Tests to make controls act like Unleashed.                                          //
// These do not work properly. Leaving them here because I might return to them later. //
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

int __stdcall compareIsLessThanOrEqualToZero(float value)
{
	return value <= 0;
}

const volatile float CSonicPostureDriftAngleThresholdLeft = -cosf(1.308996915817261f);
const volatile float CSonicPostureDriftAngleThresholdRight = cosf(1.308996915817261f);

uint32_t CSonicPostureDriftMidAsmHookReturnAddressOnFalse = 0x1199042;
uint32_t CSonicPostureDriftMidAsmHookReturnAddressOnTrue = 0x119902F;
uint32_t CSonicPostureDriftFunctionAddress = 0x9BF650;

void __declspec(naked) CSonicPostureDriftMidAsmHookLeft()
{
	__asm
	{
		call[CSonicPostureDriftFunctionAddress]
		fld[CSonicPostureDriftAngleThresholdLeft]

		push[esp + 0x28]
		call compareIsLessThanOrEqualToZero

		cmp eax, 0
		jz OnTrue

		jmp[CSonicPostureDriftMidAsmHookReturnAddressOnFalse]

    OnTrue:
		jmp[CSonicPostureDriftMidAsmHookReturnAddressOnTrue]
	}
}

void __declspec(naked) CSonicPostureDriftMidAsmHookRight()
{
    __asm
	{
		call[CSonicPostureDriftFunctionAddress]
		fld[CSonicPostureDriftAngleThresholdRight]
		fxch st(1)

		push[esp + 0x28]
		call compareIsLessThanOrEqualToZero

		cmp eax, 0
		jnz OnTrue

		jmp[CSonicPostureDriftMidAsmHookReturnAddressOnFalse]

	OnTrue:
		jmp[CSonicPostureDriftMidAsmHookReturnAddressOnTrue]
	}
}

uint32_t CSonicPostureDriftMidAsmHookCmpReturnAddress = 0x1199014;

void __declspec(naked) CSonicPostureDriftMidAsmHookCmp()
{
    __asm
	{
		fld [esp+0x28]
		fldz
		fcomip st(0), st(1)
		jmp[CSonicPostureDriftMidAsmHookCmpReturnAddress]
	}
}

//~~~~~~~~~~~~~//
// Entry Point //
//~~~~~~~~~~~~~//

extern "C" __declspec(dllexport) void __cdecl Init(ModInfo* info)
{
    std::string dir = info->CurrentMod->Path;

    size_t pos = dir.find_last_of("\\/");
    if (pos != std::string::npos)
        dir.erase(pos + 1);

    // Add drift animations to the animation list
    INSTALL_HOOK(InitializeSonicAnimationList);

    // Patch archive tree to load animation files
    INSTALL_HOOK(ParseArchiveTree);

    // Don't change to ball model during drift
    WRITE_NOP(0xDF30AB, 0xD);

    // Make hop end immediately
    static double hopHeight = 0;
    WRITE_MEMORY(0xDF2BD5 + 2, double*, &hopHeight);

    // Don't change animation in CStart state
    WRITE_NOP(0xDF2B33, 0xE);

    // Don't abruptly stop player when abort is called
    WRITE_JUMP(0xDF2CE8, CSonicStateDriftStartUpdateMidAsmHook);
    WRITE_MEMORY(0xDF2F4B, uint8_t, 0x90, 0xE9);

    // Use correct animations
    WRITE_JUMP(0xDF35E2, CSonicStateDriftDriftingLoopAnimationMidAsmHook);

    // Prevent Sonic from actually hopping
    WRITE_MEMORY(0xDF2C58, uint8_t, 0xD9, 0x54, 0x24, 0x90);

    // Don't play hop sound
    WRITE_MEMORY(0xDF2B59 + 1, int32_t, -1);

	// Play water slider sound when drifting on water
	WRITE_JUMP(0xDF369E, CSonicStateDriftDriftingSoundMidAsmHook);

	// Player surfing sound when boosting on water
	INSTALL_HOOK(CSonicStatePluginOnWaterUpdate);
	INSTALL_HOOK(MsgRestartStage);

    // Don't slow down on ground
    WRITE_JUMP(0xDF33A7, (void*)0xDF3482);

	// Keep drifting even if we hit an obstacle
	WRITE_MEMORY(0xDF3373, uint8_t, 0xEB);

	// Play footsteps when drifting
	INSTALL_HOOK(CSonicStateDriftDriftingUpdate);

	// Don't set animation when outro animation is playing (yes I know it's hacky)
	INSTALL_HOOK(ChangeAnimation);

	INSTALL_HOOK(CSonicStateDriftOnLeave);

	INSTALL_HOOK(CPlayerSpeedUpdate);

	// Attempts to simulate Sonic Unleashed's turning
	// WRITE_NOP(0x1198FDB, 2);
	// WRITE_JUMP(0x119901D, CSonicPostureDriftMidAsmHookLeft);
	// WRITE_JUMP(0x1199026, CSonicPostureDriftMidAsmHookRight);
	// WRITE_MEMORY(0x119901B, uint8_t, 0x75);
	// WRITE_JUMP(0x119900D, CSonicPostureDriftMidAsmHookCmp);
	// WRITE_MEMORY(0x119901B, uint8_t, 0x7D);

	INSTALL_HOOK(CSonicCreateAnimationStates);

	// Disable drift particle
	WRITE_MEMORY(0x15E92B4, uint8_t, 0x00);
	WRITE_MEMORY(0x15E92CC, uint8_t, 0x00);
}