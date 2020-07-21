#pragma once

#include <cstdint>

class CItemInfo;

struct CItemInfo__vftable
{
    void *destructor;
    bool (__thiscall *GetIsClassId) (CItemInfo*, uint32_t);
    uint32_t* (__thiscall *GetClassId) (CItemInfo*);
    uint32_t (__thiscall *GetModelHash) (CItemInfo *);
    void *GetParStructure;
};

class CItemInfo
{
public:
    CItemInfo__vftable *vft;
    uint8_t             field_0x8[8];
    uint32_t            Name;
    uint32_t            Model;
    uint32_t            Audio;
    uint32_t            Slot;

    /* VFT Functions (for convenience :P)  */
    inline bool
    GetIsClassId (uint32_t id)
    {
        return vft->GetIsClassId (this, id);
    }
    inline uint32_t
    GetClassId ()
    {
        return *vft->GetClassId (this);
    }
    inline uint32_t
    GetModelHash ()
    {
        return vft->GetModelHash (this);
    }
};

// Class not filled because of discrepencies between versions
class CWeaponInfo : public CItemInfo
{
};

class CWeaponInfoManager
{
public:
    char                    field_0x0[16][4];
    CWeaponInfo **          m_paInfos;
    int16_t                 m_nInfosCount;
    int16_t                 m_nInfosCapacity;
    uint8_t                 field_0x4c[4];
    struct CWeaponInfoBlob *m_paBlobs;
    uint16_t                m_nBlobsCount;
    int16_t                 m_nBlobsCapacity;
    uint8_t                 field_0x5c[36];
    uint32_t                field_0x80;
    uint8_t                 field_0x84[4];
    uint64_t                field_0x88;
    uint32_t                field_0x90;

    static CWeaponInfoManager *sm_Instance;

    inline static CWeaponInfo *
    GetInfoFromIndex (int index)
    {
        return sm_Instance->m_paInfos[index];
    }

    static void InitialisePatterns ();
};
