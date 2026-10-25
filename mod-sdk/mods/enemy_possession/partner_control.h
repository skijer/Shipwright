#ifndef NEI_PARTNER_CONTROL_V1_H
#define NEI_PARTNER_CONTROL_V1_H
// Public ABI, duplicated in each consumer so every mod builds independently.
// Publish/FindService("nei.partner.register.v1") returns the registration function.
typedef struct {
    u32 size;
    const char* key;
    const char* name;
    Actor* (*actor)(PlayState*);
    bool (*begin)(PlayState*);
    void (*end)(PlayState*);
    void (*input)(PlayState*, const Input*, s16 cameraYaw);
    u32 flags; // Appended: omitted by older providers. Bit 0 tracks/hides Link inside the controlled actor.
} NeiPartnerV1;
#define NEI_PARTNER_TRACK_PLAYER 1u
typedef bool (*NeiRegisterPartnerV1)(const NeiPartnerV1*);
#endif
