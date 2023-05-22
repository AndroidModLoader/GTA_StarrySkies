#include <mod/amlmod.h>
#include <mod/config.h>
#include <mod/logger.h>
#include <ctime>

#include "GTASA_STRUCTS.h"

MYMOD(net.rusjj.starryskies, GTA StarrySkies, 1.1, RusJJ)
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.aml, 1.0.2.1)
END_DEPLIST()

#define AMOUNT_OF_STARS 100
#define STAR_SKYBOX_SIDES 5

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Saves     ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
uintptr_t pGTASA;
void* hGTASA;
float StarCoorsX[STAR_SKYBOX_SIDES][AMOUNT_OF_STARS], StarCoorsY[STAR_SKYBOX_SIDES][AMOUNT_OF_STARS], StarSizes[STAR_SKYBOX_SIDES][AMOUNT_OF_STARS]; // 5 - sides
float fSmallStars, fMiddleStars, fBiggestStars, fBiggestStarsSpawnChance;
CVector PositionsTable[5] =
{
    { 100.0f,  0.0f,   10.0f}, // Left
    {-100.0f,  0.0f,   10.0f}, // Right
    {   0.0f,  100.0f, 10.0f}, // Front
    {   0.0f, -100.0f, 10.0f}, // Back
    {   0.0f,  0.0f,   95.0f}, // Up
};

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Vars      ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CCamera* TheCamera;

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Funcs     ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
bool (*CalcScreenCoors)(CVector*, CVector*, float*, float*, bool, bool);
void (*RenderBufferedOneXLUSprite)(CVector pos, float w, float h, uint8_t r, uint8_t g, uint8_t b, short intens, float recipZ, uint8_t a);

/////////////////////////////////////////////////////////////////////////////
//////////////////////////////     Patches     //////////////////////////////
/////////////////////////////////////////////////////////////////////////////
uintptr_t StarrySkies_BackTo;
extern "C" void StarrySkies_Patch(float intensity)
{
    CVector ScreenPos, WorldPos, WorldStarPos, CamPos = TheCamera->GetPosition();
    float SZ, SZX, SZY;

    for(int side = 0; side < STAR_SKYBOX_SIDES; ++side)
    {
        WorldPos = PositionsTable[side] + CamPos;
        for(int i = 0; i < AMOUNT_OF_STARS; ++i)
        {
            WorldStarPos = WorldPos;
            SZ = StarSizes[side][i];
            switch(side)
            {
                case 0:
                case 1:
                    WorldStarPos.y -= StarCoorsX[side][i];
                    WorldStarPos.z += StarCoorsY[side][i];
                    break;

                case 2:
                case 3:
                    WorldStarPos.x -= StarCoorsX[side][i];
                    WorldStarPos.z += StarCoorsY[side][i];
                    break;

                default:
                    WorldStarPos.x += StarCoorsX[side][i];
                    WorldStarPos.y += StarCoorsY[side][i];
                    break;
            }

            if(CalcScreenCoors(&WorldStarPos, &ScreenPos, &SZX, &SZY, false, true))
            {
                uint8_t brightness = (1.0 - 0.015f * (rand() & 0x1F)) * intensity;
                RenderBufferedOneXLUSprite(ScreenPos, SZX * SZ, SZY * SZ,
                                           brightness, brightness, brightness, 255, 1.0f / ScreenPos.z, 255);
            }
        }
    }
}
__attribute__((optnone)) __attribute__((naked)) void StarrySkies_inject(void)
{
    asm volatile(
        "PUSH            {R0-R11}\n"
        "VMOV            R0, S20\n"
        "VPUSH           {S0-S31}\n"
        "BL              StarrySkies_Patch\n"
        "VPOP            {S0-S31}\n");
    asm volatile(
        "MOV             R12, %0\n"
        "POP             {R0-R11}\n"
        "BX              R12\n"
    :: "r" (StarrySkies_BackTo));
}

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Funcs     ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
inline float RandomIt(float min, float max)
{
    return (((float)rand()) / (float)RAND_MAX) * (max - min) + min;
}
void InitializeThoseStars()
{
    // Keeps stars always the same
    srand(cfg->GetInt("StarsSeed", 0xBEEF));

    for(int side = 0; side < STAR_SKYBOX_SIDES; ++side)
    for(int i = 0; i < AMOUNT_OF_STARS; ++i)
    {
        StarCoorsX[side][i] = 95.0f * RandomIt(-1.0f, 1.0f);

        // Side=4 is when rendering stars directly ABOVE us
        if(side == 4) StarCoorsY[side][i] = 95.0f * RandomIt(-1.0f, 1.0f);
        else StarCoorsY[side][i] = 95.0f * RandomIt(-0.35f, 1.0f);

        // Smaller chances for a bigger star (this is more life-like)
        if(RandomIt(0.0f, 1.0f) > fBiggestStarsSpawnChance) StarSizes[side][i] = 0.8f * RandomIt(fSmallStars, fBiggestStars);
        else StarSizes[side][i] = 0.8f * RandomIt(fSmallStars, fMiddleStars);
    }

    // Makes other rand() calls "more random"
    srand(time(NULL));
}
inline float ClampFloat(float value, float min, float max)
{
    if(value > max) return max;
    if(value < min) return min;
    return value;
}
extern "C" void OnModPreLoad()
{
    logger->SetTag("StarrySkies");
    
    pGTASA = aml->GetLib("libGTASA.so");
    hGTASA = aml->GetLibHandle("libGTASA.so");
    
    // GTA Variables
    SET_TO(TheCamera, aml->GetSym(hGTASA, "TheCamera"));

    // GTA Functions
    SET_TO(CalcScreenCoors, aml->GetSym(hGTASA, "_ZN7CSprite15CalcScreenCoorsERK5RwV3dPS0_PfS4_bb"));
    SET_TO(RenderBufferedOneXLUSprite, aml->GetSym(hGTASA, "_ZN7CSprite26RenderBufferedOneXLUSpriteEfffffhhhsfh"));

    // GTA Patches
    StarrySkies_BackTo = pGTASA + 0x59F20A + 0x1; // 0x59F134
    aml->Redirect(pGTASA + 0x59F002 + 0x1, (uintptr_t)StarrySkies_inject);

    // Do init stuffs
    fSmallStars = ClampFloat(cfg->GetFloat("SmallestStarsSize", 0.15f), 0.03f, 2.5f);
    fMiddleStars = ClampFloat(cfg->GetFloat("MiddleStarsSize", 0.6f), 0.03f, 2.5f);
    fBiggestStars = ClampFloat(cfg->GetFloat("BiggestStarsSize", 1.2f), 0.03f, 2.5f);
    fBiggestStarsSpawnChance = 1.0f - 0.01f * ClampFloat(cfg->GetFloat("BiggestStarsChance", 20), 0.0f, 100.0f);
    InitializeThoseStars();
}

Config cfgLocal("StarrySkies"); Config* cfg = &cfgLocal;